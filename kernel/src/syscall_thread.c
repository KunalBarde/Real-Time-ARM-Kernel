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

/* TCB Buffer Size (16 Threads Max)*/
#define MAX_THREADS 16
#define BUFFER_SIZE MAX_THREADS+1
#define WORD_SIZE 4
#define K_BLOCK_SIZE (sizeof(k_threading_state_t))
#define TCB_BUFFER_SIZE (sizeof(tcb_t) * (BUFFER_SIZE)) //Avoiding zero indexing

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
static volatile char kernel_wait_set[BUFFER_SIZE] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};

/* Add threads to ready set once sys_thread_create is called */
static volatile char kernel_ready_set[BUFFER_SIZE];

/* PendSV handler moves threads to running */
static volatile char kernel_running_set[BUFFER_SIZE];

/* Thread specific state */
static volatile char tcb_buffer[TCB_BUFFER_SIZE];

/* Static global thread id assignment */
static volatile int thread_idx = 1;

/**
* Count of systick interrupt fires. 
*/
volatile uint32_t sys_ticks = 0;

/**
* @brief	Handler called in occassion of sys-tick interrupt
*/
void systick_c_handler() {
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
  k_threading_state_t *_kernel_state_block;
  
  /* Check if proposed stack size can fit in kernel/user stack space */

  uint32_t stack_size_bytes = mm_log2ceil_size(stack_size) * WORD_SIZE;
  uint32_t user_stack_thresh = __thread_u_stacks_low - __thread_u_stacks_top;
  uint32_t kernel_stack_thresh = __thread_k_stacks_low - __thread_k_stacks_top;

  if(stack_size_bytes > user_stack_thresh || stack_size_bytes > kernel_stack_thresh)
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
  _kernel_state_block->idle_fn = idle_fn;
  _kernel_state_block->mem_prot = memory_protection;

  return 0;
}

int sys_thread_create(
  void *fn,
  uint32_t prio,
  uint32_t C,
  uint32_t T,
  void *vargp
){
  (void) fn; (void) prio; (void) C; (void) T; (void) vargp;
  tcb_t *base_ptr;
  (void) base_ptr;

  base_ptr = (tcb_t*)((char *)tcb_buffer+(sizeof(tcb_t)*(thread_idx++)));
  //TODO: Set TCB entry for particular thread id
  
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
