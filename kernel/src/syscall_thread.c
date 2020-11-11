/** @file   syscall_thread.c
 *
 *  @brief	System calls used for multithreading purposes.   
 *
 *  @date   
 *
 *  @author	Kunal Barde, Nick Toldalagi
 */

#include <stdint.h>
#include "syscall_thread.h"
#include "syscall_mutex.h"
#include "syscall.h"
#include "mpu.h"
#include <timer.h>
#include <arm.h>

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
#define I_THREAD_IDX 14 //Idle thread index into kernel buffers
#define D_THREAD_IDX 15 //Default thread index into kernel buffers
#define I_THREAD_PRIOR 14 //Lowest priority initially 
#define D_THREAD_PRIOR 15 //Not really lowest priority, not a normal thread
#define INIT 0
#define WAITING 1 /**< Waiting state for a thread*/
#define RUNNABLE 2
#define RUNNING 3


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
static volatile char kernel_wait_set[BUFFER_SIZE] = {0};

/* Add threads to ready set once sys_thread_create is called */
static volatile char kernel_ready_set[BUFFER_SIZE] = {0};

/* PendSV handler moves threads to running */
//static volatile char kernel_running_set[BUFFER_SIZE] = {0};

/* Thread specific state */
static volatile tcb_t tcb_buffer[BUFFER_SIZE];

/* Static global thread id assignment */
static volatile int thread_idx = 0;

/**
 * @brief	Performs a UB schedulability test on a new thread being added to the task set. 

 * @param[in]	T	Period of new Thread.
 * @param[in]	C	Worst case runtime of new thread. 

 * @return	0 if schedulable, -1 otherwise.
 */
int ub_test(float T, float C) {
   k_threading_state_t *kcb = (k_threading_state_t *)kernel_threading_state;
   float u_tot = C/T;
   for(int i = 0; i < MAX_U_THREADS; i++) {
      if(tcb_buffer[i].thread_state != INIT) {
         u_tot += tcb_buffer[i].U;
      }
   }
   if(u_tot <= ub_table[kcb->u_thread_ct+1]) return -1;
   return 0;
}

/**
* @brief	Handler called in occassion of sys-tick interrupt
*/
void systick_c_handler() {
  k_threading_state_t *_kernel_state_block = (k_threading_state_t *)kernel_threading_state;

  _kernel_state_block->sys_tick_ct++;
  uint8_t curr_thread = _kernel_state_block->running_thread;
  tcb_buffer[curr_thread].duration++;
  
  if(tcb_buffer[curr_thread].duration >= tcb_buffer[curr_thread].C) {
    tcb_buffer[curr_thread].thread_state = WAITING;
  }

  for(int i = 0; i < MAX_U_THREADS; i++) {
    if(tcb_buffer[i].thread_state != INIT) {
       tcb_buffer[i].period_ct++;
       if(tcb_buffer[i].period_ct >= tcb_buffer[i].T) {
          tcb_buffer[i].period_ct = 0;
          tcb_buffer[i].duration = 0;
          tcb_buffer[i].thread_state = RUNNABLE;
       }
    }

  }
   
  //Set PendSV to pending
  pend_pendsv();
  return;

}

/**
 * @brief	Implementation of round robin scheduler

 * @param[in]	Context pointer of current thread

 * @return	PSP of next context to load and run. If there are no more user threads left to run, the default thread's context is returned. 
 */
thread_stack_frame *round_robin(void *curr_context_ptr) {
  k_threading_state_t *_kernel_state_block = (k_threading_state_t *)kernel_threading_state;

  int8_t running_buf_idx = _kernel_state_block->running_thread;

  //Save current context and move back to ready set
  tcb_buffer[running_buf_idx].kernel_stack_ptr = (uint32_t) curr_context_ptr;
  tcb_buffer[running_buf_idx].svc_state = get_svc_status();
  int8_t old_running_buf_idx = running_buf_idx;

  uint32_t old_running_ready_set_idx = tcb_buffer[old_running_buf_idx].priority;
  uint32_t running_ready_set_idx = tcb_buffer[running_buf_idx].priority;

  running_ready_set_idx = (running_ready_set_idx >= I_THREAD_PRIOR-1) ? 0 : running_ready_set_idx + 1;

  while (1) {
    if(running_ready_set_idx == old_running_ready_set_idx) {
      running_buf_idx = old_running_buf_idx;
      break; //Done we just go back to the old thread
    }

    int curr_buf_idx = _kernel_state_block -> ready_set[running_ready_set_idx];

    if(curr_buf_idx > -1) { //found next task to run
      running_buf_idx = curr_buf_idx;
      running_ready_set_idx = tcb_buffer[running_buf_idx].priority;
      break;
    }
    
      running_ready_set_idx = (running_ready_set_idx >= I_THREAD_PRIOR-1) ? 0 : running_ready_set_idx + 1;
  }

  //Remove new running task from ready set
  _kernel_state_block -> ready_set[running_ready_set_idx] = -1;
  
  //Add old back task back to ready set
  uint32_t old_priority = tcb_buffer[old_running_buf_idx].priority;
  _kernel_state_block -> ready_set[old_priority] = old_running_buf_idx;
 
  //Set new running thread 
  _kernel_state_block->running_thread = running_buf_idx;

  set_svc_status(tcb_buffer[running_buf_idx].svc_state);
  return (thread_stack_frame *)tcb_buffer[running_buf_idx].kernel_stack_ptr;
}

/**
 * @brief	PendSV interrupt handler. Runs a scheduler and then dispatches a new thread for running. 

 * @param[in]	context_ptr	A pointer to the current thread's stack-saved context. 

 * @return	A pointer to the next thread's stack-saved context. 
 */
void *pendsv_c_handler(void *context_ptr) {
  thread_stack_frame *context = (thread_stack_frame *)context_ptr;
  context = (thread_stack_frame *)round_robin(context_ptr);

  return (void *)context;
}

/** 
 * @brief	System call to initialize a new thread. 

 * @return	0 on success -1 otherwise. 
 */
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

  uint32_t user_stack_thresh = (uint32_t)(&__thread_u_stacks_top) - (uint32_t)(&__thread_u_stacks_low);

  uint32_t kernel_stack_thresh = (uint32_t)(&__thread_k_stacks_top) - (uint32_t)(&__thread_k_stacks_low);
  breakpoint();
  uint32_t stack_consumption = (max_threads+1)*stack_size_bytes;
  
  if(stack_consumption > user_stack_thresh || stack_consumption > kernel_stack_thresh)
     return -1; 

  /* Initialize kernel data structures for threading */  
 
  _kernel_state_block = (k_threading_state_t *)kernel_threading_state;
  _kernel_state_block->wait_set = (uint8_t *)kernel_wait_set;
  _kernel_state_block->ready_set = (uint8_t *)kernel_ready_set;
  //_kernel_state_block->running_set = (uint8_t *)kernel_running_set; 
  _kernel_state_block->running_thread = D_THREAD_PRIOR;
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
     breakpoint();
     user_stack_brk = user_stack_brk - stack_size_bytes;
     tcb_buffer[i].kernel_stack_ptr = kernel_stack_brk;
     kernel_stack_brk = kernel_stack_brk - stack_size_bytes;
     tcb_buffer[i].thread_state = INIT;
     tcb_buffer[i].svc_state = 0;
     tcb_buffer[i].U = 0;
  }
  
  /* Set kernel state for idle thread 14 */
  tcb_buffer[I_THREAD_IDX].user_stack_ptr = user_stack_brk;
  tcb_buffer[I_THREAD_IDX].kernel_stack_ptr = kernel_stack_brk;
  tcb_buffer[I_THREAD_IDX].U = 0;
  tcb_buffer[I_THREAD_IDX].thread_state = WAITING;
  
  /* Set kernel state for default thread 15 */
  tcb_buffer[D_THREAD_IDX].thread_state = RUNNABLE;
  tcb_buffer[D_THREAD_IDX].svc_state = 0;
  tcb_buffer[D_THREAD_IDX].U = 0;

  /* Move idle thread to runnable*/
  if(idle_fn == NULL) {
    sys_thread_create(wait_for_interrupt, I_THREAD_PRIOR, 0, 1, NULL);
    return 0;
  }
  breakpoint();
  sys_thread_create(idle_fn, I_THREAD_PRIOR, 0, 1, NULL);
  return 0;
}

extern void thread_kill(void);

/**
 * @brief	System call to spawn a new thread. 

 * @return	0 on success -1 otherwise. 
 */
int sys_thread_create(
  void *fn,
  uint32_t priortiy,
  uint32_t C,
  uint32_t T,
  void *vargp
){

  uint32_t user_stack_ptr = tcb_buffer[prio].user_stack_ptr - sizeof(interrupt_stack_frame);
  interrupt_stack_frame *interrupt_frame = (interrupt_stack_frame *)user_stack_ptr;

  uint32_t kernel_stack_ptr = tcb_buffer[prio].kernel_stack_ptr - sizeof(thread_stack_frame);
  thread_stack_frame *thread_frame = (thread_stack_frame *)kernel_stack_ptr;
  
  breakpoint();
  interrupt_frame->r0 = (unsigned int)vargp;
  interrupt_frame->r1 = 0;
  interrupt_frame->r2 = 0;
  interrupt_frame->r3 = 0;
  interrupt_frame->r12 = 0;
//  interrupt_frame->lr = (unsigned int)&thread_kill;
  interrupt_frame->pc = (uint32_t)fn;
  interrupt_frame->xPSR = XPSR_INIT;

  tcb_buffer[prio].user_stack_ptr = user_stack_ptr;
  tcb_buffer[prio].C = C;
  tcb_buffer[prio].T = T;
  
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

  k_threading_state_t *ksb = (k_threading_state_t *)kernel_threading_state;
  if(prio != I_THREAD_IDX) ksb->u_thread_ct++;
    
  return 0;
}

/**
 * @brief	System call to begin the rtos scheduler. Pends a PendSV so this function will only return when all previously scheduled user threads have been killed or have terminated.  

 * @param[in]	frequency	The frequency (in Hz) at which the the Systick interrupt should fire in order to re-run the scheduler to evaluate the current working pool of threads. 

 * @return	0 on succes -1 on failure. 
 */
int sys_scheduler_start( uint32_t frequency ){
  uint32_t timer_period = CPU_CLK_FREQ/frequency;
  k_threading_state_t *ksb = (k_threading_state_t *)kernel_threading_state;
  ksb -> sys_tick_ct = 0;
  timer_start(timer_period);

  pend_pendsv(); //Begin first thread
  
  return 0;
}

/**
 * @brief	Returns priority of current running thread. 

 * @brief	Priority of current running thread. 
 */
uint32_t sys_get_priority(){
  k_threading_state_t *ksb = (k_threading_state_t *)kernel_threading_state;

  return tcb_buffer[ksb->running_thread].priority;
}

/** 
 * @brief	Returns number of system ticks since scheduler was initialized. 

 * @return	The number of system ticks since this scheduler was initialized. 
 */
uint32_t sys_get_time(){
   k_threading_state_t *ksb = (k_threading_state_t *)kernel_threading_state;

  return ksb->sys_tick_ct;
}

/** 
 * @brief	Returns the amount of actual execution time consumed by a the current thread. 

 * @return	The duration of execution of the current thread. 
 */ 
uint32_t sys_thread_time(){
  k_threading_state_t *ksb = (k_threading_state_t *)kernel_threading_state;
  return tcb_buffer[ksb->running_thread].duration;
}

void sys_thread_kill(){
  k_threading_state_t *ksb = (k_threading_state_t *)kernel_threading_state;
  if(ksb->running_thread == I_THREAD_IDX) {
    tcb_buffer[I_THREAD_IDX].thread_state = INIT;
    sys_thread_create(wait_for_interrupt, I_THREAD_PRIOR, 0, 1, NULL);
  }

  if(ksb->running_thread == D_THREAD_IDX) {
    sys_exit(0);
    return;
  }
  tcb_buffer[ksb->running_thread].thread_state = INIT;
  if(ksb->running_thread != I_THREAD_IDX) ksb->u_thread_ct++;
  pend_pendsv();
  return;
}

void sys_wait_until_next_period(){
  k_threading_state_t *ksb = (k_threading_state_t *)kernel_threading_state;
  tcb_buffer[ksb->running_thread].thread_state = WAITING;
  return;
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
