/** @file   syscall_thread.c
 *
 *  @brief  
 *
 *  @date   
 *
 *  @author 
 */

#include <stdint.h>
#include "syscall_thread.h"
#include "syscall_mutex.h"
#include "mpu.h"

/** @brief      Initial XPSR value, all 0s except thumb bit. */
#define XPSR_INIT 0x1000000

/** @brief Interrupt return code to user mode using PSP.*/
#define LR_RETURN_TO_USER_PSP 0xFFFFFFFD
/** @brief Interrupt return code to kernel mode using MSP.*/
#define LR_RETURN_TO_KERNEL_MSP 0xFFFFFFF1

#define MAX_TOTAL_THREADS 16
#define MAX_U_THREADS 14
#define BUFFER_SIZE MAX_TOTAL_THREADS
#define WORD_SIZE 4
#define K_BLOCK_SIZE (sizeof(k_threading_state_t))
#define TCB_BUFFER_SIZE (sizeof(tcb_t) * (BUFFER_SIZE)) //Avoiding zero indexing
#define I_THREAD_IDX 0 //Idle thread index into kernel buffers
#define D_THREAD_IDX 1 //Default thread index into kernel buffers
#define I_THREAD_PRIOR 15 //Lowest priority initially 
#define WAITING 0
#define RUNNABLE 1
#define RUNNING 2


/**
 * @brief      Heap high and low pointers.
 */
//@{
extern char
  __thread_u_stacks_low,
  __thread_u_stacks_top,
  __thread_k_stacks_low,
  __thread_k_stacks_top;
//@}

/**
 * @brief      Precalculated values for UB test.
 */
float ub_table[] = {
  0.000, 1.000, .8284, .7798, .7568,
  .7435, .7348, .7286, .7241, .7205,
  .7177, .7155, .7136, .7119, .7106,
  .7094, .7083, .7075, .7066, .7059,
  .7052, .7047, .7042, .7037, .7033,
  .7028, .7025, .7021, .7018, .7015,
  .7012, .7009
};

/**
 * @struct user_stack_frame
 *
 * @brief  Stack frame upon exception.
 */
typedef struct {
  uint32_t r0;   /** @brief Register value for r0 */
  uint32_t r1;   /** @brief Register value for r1 */
  uint32_t r2;   /** @brief Register value for r2 */
  uint32_t r3;   /** @brief Register value for r3 */
  uint32_t r12;  /** @brief Register value for r12 */
  uint32_t lr;   /** @brief Register value for lr*/
  uint32_t pc;   /** @brief Register value for pc */
  uint32_t xPSR; /** @brief Register value for xPSR */
} interrupt_stack_frame;

/* Kernel Data Structures */

/* Global threading state */
static volatile char kernel_threading_state[K_BLOCK_SIZE];

/* Initially all threads should be in the wait set */
static volatile char kernel_wait_set[BUFFER_SIZE] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};

/* Add threads to ready set once sys_thread_create is called */
static volatile char kernel_ready_set[BUFFER_SIZE];

/* PendSV handler moves threads to running */
static volatile char kernel_running_set[BUFFER_SIZE];

/* Thread specific state */
static volatile tcb_t tcb_buffer[BUFFER_SIZE];

/* Static global thread id assignment */
static volatile int thread_idx = 0;

/**
* @brief	Handler called in occassion of sys-tick interrupt
*/
void systick_c_handler() {
  k_threading_state_t *_kernel_state_block = (k_threading_state_t *)kernel_threading_state;

  _kernel_state_block->sys_tick_ct++;
  return;

}

void *pendsv_c_handler(void *context_ptr){
  (void) context_ptr;

  return NULL;
}

int sys_thread_init(
  uint32_t max_threads,
  uint32_t stack_size,
  void *idle_fn,
  protection_mode memory_protection,
  uint32_t max_mutexes
){
  (void) max_threads; (void) stack_size; (void) idle_fn;
  (void) memory_protection; (void) max_mutexes;
  
  //User can only allocate up to and including 14 threads
  if(max_threads > MAX_U_THREADS) return -1;

  k_threading_state_t *_kernel_state_block;
  
  /* Check if proposed stack size can fit in kernel/user stack space */
  uint32_t stack_size_bytes = (1<<(mm_log2ceil_size(stack_size*WORD_SIZE)));
  uint32_t user_stack_thresh = (uint32_t)(&__thread_u_stacks_low) - (uint32_t)(&__thread_u_stacks_top);

  uint32_t kernel_stack_thresh = (uint32_t)(&__thread_k_stacks_low) - (uint32_t)(&__thread_k_stacks_top);
  
  uint32_t stack_consumption = (max_threads+1)*stack_size_bytes;
  
  if(stack_consumption > user_stack_thresh || stack_consumption > kernel_stack_thresh)
     return -1; 

  /* Initialize kernel data structures for threading */  
 
  _kernel_state_block = (k_threading_state_t *)kernel_threading_state;
  _kernel_state_block->wait_set = (uint8_t *)kernel_wait_set;
  _kernel_state_block->ready_set = (uint8_t *)kernel_ready_set;
  _kernel_state_block->running_set = (uint8_t *)kernel_running_set;
  _kernel_state_block->sys_tick_ct = 0;
  _kernel_state_block->stack_size = stack_size;
  _kernel_state_block->max_threads = max_threads;
  _kernel_state_block->max_mutexes = max_mutexes;
  _kernel_state_block->mem_prot = memory_protection;
  _kernel_state_block->u_thread_ct = 0;

  uint32_t user_stack_brk = (uint32_t)&__thread_u_stacks_top;
  uint32_t kernel_stack_brk = (uint32_t)&__thread_k_stacks_top;
  
  /* Divide Up User & Kernel Space Stacks For User Threads */
  for(size_t i = 0; i < max_threads; i++) {
     tcb_buffer[i].user_stack_ptr = user_stack_brk;
     user_stack_brk = user_stack_brk - stack_size_bytes;
     tcb_buffer[i].kernel_stack_ptr = kernel_stack_brk;
     kernel_stack_brk = kernel_stack_brk - stack_size_bytes;
     tcb_buffer[i].thread_state = WAITING;
     tcb_buffer[i].svc_state = 0;
     tcb_buffer[i].U = 0;
  }
  
  /* Set kernel state for idle thread 0 */
  tcb_buffer[I_THREAD_IDX].user_stack_ptr = user_stack_brk;
  tcb_buffer[I_THREAD_IDX].kernel_stack_ptr = kernel_stack_brk;
  tcb_buffer[I_THREAD_IDX].U = 0;
  tcb_buffer[I_THREAD_IDX].thread_state = WAITING;
  
  /* Set kernel state for default thread 1 */
  tcb_buffer[D_THREAD_IDX].thread_state = RUNNABLE;
  tcb_buffer[D_THREAD_IDX].svc_state = 0;
  tcb_buffer[D_THREAD_IDX].U = 0;

  /* Move idle thread to runnable*/
  sys_thread_create(idle_fn, I_THREAD_PRIOR, 0, 1, NULL);

  return 0;
}

extern void thread_kill(void);

int sys_thread_create(
  void *fn,
  uint32_t prio,
  uint32_t C,
  uint32_t T,
  void *vargp
){
  (void) fn; (void) prio; (void) C; (void) T; (void) vargp;
   
//  extern void thread_kill(void);

  uint32_t user_stack_ptr = tcb_buffer[prio].user_stack_ptr - sizeof(interrupt_stack_frame);
  interrupt_stack_frame *interrupt_frame = (interrupt_stack_frame *)user_stack_ptr;

  uint32_t kernel_stack_ptr = tcb_buffer[prio].kernel_stack_ptr - sizeof(thread_stack_frame);
  thread_stack_frame *thread_frame = (thread_stack_frame *)kernel_stack_ptr;
  
  interrupt_frame->r0 = (unsigned int)vargp;
  interrupt_frame->r1 = 0;
  interrupt_frame->r2 = 0;
  interrupt_frame->r3 = 0;
  interrupt_frame->r12 = 0;
//  interrupt_frame->lr = (unsigned int)&thread_kill;
  interrupt_frame->pc = (uint32_t)fn;
  interrupt_frame->xPSR = XPSR_INIT;

  tcb_buffer[prio].user_stack_ptr = user_stack_ptr;
  
  thread_frame->r4 = 0;
  thread_frame->r5 = 0;
  thread_frame->r6 = 0;
  thread_frame->r7 = 0;
  thread_frame->r8 = 0;
  thread_frame->r9 = 0;
  thread_frame->r10 = 0;
  thread_frame->r11 = 0;
  thread_frame->r14 = LR_RETURN_TO_USER_PSP;

  /* Mark thread as runnable */
  kernel_ready_set[prio] = RUNNABLE;

  k_threading_state_t *kernel_state = (k_threading_state_t *)kernel_threading_state;
  if(prio != I_THREAD_IDX) kernel_state->u_thread_ct++;
    
  return -1;
}

int sys_scheduler_start( uint32_t frequency ){
  (void) frequency;
  return -1;
}

uint32_t sys_get_priority(){
  return 0U;
}

uint32_t sys_get_time(){
  return 0U;
}

uint32_t sys_thread_time(){
  return 0U;
}

void sys_thread_kill(){
}

void sys_wait_until_next_period(){
}

kmutex_t *sys_mutex_init( uint32_t max_prio ) {
  (void) max_prio;
  return NULL;
}

void sys_mutex_lock( kmutex_t *mutex ) {
  (void) mutex;
}

void sys_mutex_unlock( kmutex_t *mutex ) {
  (void) mutex;
}
