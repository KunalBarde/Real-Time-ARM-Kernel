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

#define MAX_TOTAL_THREADS 16 /**< Maximum total threads allowed by the system*/
#define MAX_U_THREADS 14 /**< Maximum number of user alloated threads*/

#define BUFFER_SIZE MAX_TOTAL_THREADS /**< Thread control block buffer size (in number of tcbs)*/

#define WORD_SIZE 4 /**< System word size*/

#define K_BLOCK_SIZE (sizeof(k_threading_state_t)) /**< sizeof(k_thread_state_t)*/

#define TCB_BUFFER_SIZE (sizeof(tcb_t) * (BUFFER_SIZE)) /**< Thread control block buffer size (in bytes)*/

#define I_THREAD_SET_IDX 14 /**<Idle thread index into kernel buffers*/
#define D_THREAD_SET_IDX 15 /**<Default thread index into kernel buffers*/

#define I_THREAD_PRIORITY 14 /**<Priority of idle thread*/
#define D_THREAD_PRIORITY 15 /**<Priority of default thread*/
#define INIT 0 /**< Initialization state for a thread */
#define WAITING 1 /**< Waiting state for a thread*/
#define RUNNABLE 2 /**< Runnable state for a thread*/
#define RUNNING 3 /**< Running state for thread*/


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
 * @brief	Default idle function asm stub needed for sleeping the processor when all other threads are waiting. Used if the user idle function exits or is never provided in thread_init. 
 */
extern void default_idle();

/**
 * @brief	Sys_kill asm svc stub. Used for having finished threads automatically call sys_kill on themselves upon returning from their work. 
 */
extern void _kill();

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


/* Kernel Data Structures */

/* Global threading state */
static volatile char kernel_threading_state[K_BLOCK_SIZE];

/* Initially all threads should be in the wait set */
static volatile signed char kernel_wait_set[BUFFER_SIZE] = {0};

/* Add threads to ready set once sys_thread_create is called */
static volatile signed char kernel_ready_set[BUFFER_SIZE] = {0};

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
 * @brief	Updates kernel ready and waiting set structures to reflect thread states accurately.
 */
void update_kernel_sets() {
  k_threading_state_t *ksb = (k_threading_state_t *)kernel_threading_state;
  //breakpoint();
  for(uint32_t i = 0; i < ksb->max_threads; i++) { //TODO" update to all threads
    int8_t cur_set_idx = tcb_buffer[i].priority;
    //breakpoint();
    switch(tcb_buffer[i].thread_state) {
      case WAITING:
        ksb->ready_set[cur_set_idx] = -1;
        ksb->wait_set[cur_set_idx] = i;
        break;
      
      case RUNNING:
      case RUNNABLE:
        
        ksb->ready_set[cur_set_idx] = i;
        ksb->wait_set[cur_set_idx] = -1; //problem line 
        break;

      default: //init
        ksb->ready_set[cur_set_idx] = -1;
        ksb->wait_set[cur_set_idx] = -1;
        break;
    }
  }
  //Handle idle and default thread
  //uint8_t i_thread_state = tcb_buffer[ksb->max_threads].thread_state;
  //uint8_t d_thread_state = tcb_buffer[ksb->max_threads+1].thread_state;
  /*
  if(i_thread_state == WAITING) {
    ksb->ready_set[I_THREAD_SET_IDX] = -1;
    ksb->wait_set[I_THREAD_SET_IDX] = ksb->max_threads;
  } else if(i_thread_state) {
    ksb->ready_set[I_THREAD_SET_IDX] = ksb->max_threads;
    ksb->wait_set[I_THREAD_SET_IDX] = -1;
  } else {
    ksb->ready_set[I_THREAD_SET_IDX] = -1;
    ksb->wait_set[I_THREAD_SET_IDX] = -1;
  } 

  if(d_thread_state == WAITING) {
    ksb->ready_set[D_THREAD_SET_IDX] = -1;
    ksb->wait_set[D_THREAD_SET_IDX] = ksb->max_threads+1;
  } else if(i_thread_state) {
    ksb->ready_set[D_THREAD_SET_IDX] = ksb->max_threads+1;
    ksb->wait_set[D_THREAD_SET_IDX] = -1;
  } else {
    ksb->ready_set[D_THREAD_SET_IDX] = -1;
    ksb->wait_set[D_THREAD_SET_IDX] = -1;
  } */
}

/**
 * @brief	Responsible for the enforcement of RMS. Will move threads between RUNNABLE and WAITING states as necessary and will handle the necessary book keeping. 

 * @param	curr_thread	Tcb_buffer idx of the currently running thread. 
 */
void update_thread_states(uint8_t curr_thread) {
  k_threading_state_t *ksb = (k_threading_state_t *)kernel_threading_state;
  
  //Yields upon finishing execution
  if(tcb_buffer[curr_thread].duration >= tcb_buffer[curr_thread].C) {
    //breakpoint();
    if(curr_thread < ksb->max_threads) { //Only user threads can be downgraded
      tcb_buffer[curr_thread].thread_state = WAITING;
      //breakpoint();
    }
  }
  //breakpoint();
  for(uint8_t i = 0; i < ksb->max_threads; i++) { //TODO: update
    if(tcb_buffer[i].thread_state != INIT) {
      tcb_buffer[i].period_ct++;
      if(tcb_buffer[i].period_ct >= tcb_buffer[i].T) {
         //breakpoint();
         tcb_buffer[i].period_ct = 0;
         tcb_buffer[i].duration = 0;
         tcb_buffer[i].thread_state = RUNNABLE;
      }
    }
  }
}

/**
* @brief	Handler called in occassion of sys-tick interrupt
                Implements RMS Scheduling
*/
void systick_c_handler() {
  k_threading_state_t *ksb = (k_threading_state_t *)kernel_threading_state;
  //breakpoint();
  ksb->sys_tick_ct++;
  uint8_t curr_thread = ksb->running_thread;
  tcb_buffer[curr_thread].duration++;
  tcb_buffer[curr_thread].total_time++;

  update_thread_states(curr_thread);  
  
  //breakpoint();
  
  //Set PendSV to pending
  /*if(tcb_buffer[curr_thread].duration >= tcb_buffer[curr_thread].C) {
    if(curr_thread != ksb->max_threads) breakpoint();
  }*/
  
  pend_pendsv();
  return;
}

/**
 * @brief	Implementation of round robin scheduler

 * @param[in]	curr_context_ptr	Context pointer of current thread

 * @return	PSP of next context to load and run. If there are no more user threads left to run, the default thread's context is returned. 
 */
void *round_robin(void *curr_context_ptr) {

  k_threading_state_t *ksb = (k_threading_state_t *)kernel_threading_state;

  int32_t running_buf_idx = ksb->running_thread;
  //Save current context
  tcb_buffer[running_buf_idx].kernel_stack_ptr = curr_context_ptr;
  tcb_buffer[running_buf_idx].svc_state = get_svc_status();

  int32_t old_running_buf_idx = running_buf_idx;

  uint32_t running_ready_set_idx = tcb_buffer[running_buf_idx].priority;

  uint32_t old_running_ready_set_idx = running_ready_set_idx;

  running_ready_set_idx = (running_ready_set_idx >= I_THREAD_SET_IDX-1) ? 0 : running_ready_set_idx + 1;
  //breakpoint();
  while (1) {
    if(running_ready_set_idx == old_running_ready_set_idx) {
      //Done we just go back to the old thread
      return tcb_buffer[old_running_buf_idx].kernel_stack_ptr;
    }

    int curr_buf_idx = ksb -> ready_set[running_ready_set_idx];

    if(curr_buf_idx > -1) { //Found next task to run
      running_buf_idx = curr_buf_idx;
      running_ready_set_idx = tcb_buffer[running_buf_idx].priority;
      break;
    }
    
    //Increment to next valid index in ready set
    running_ready_set_idx = (running_ready_set_idx >= I_THREAD_PRIORITY-1) ? 0 : running_ready_set_idx + 1;
  }

  //Remove new running task from ready set
  tcb_buffer[running_buf_idx].thread_state = RUNNING;
  
  //Add old back task back to ready set
  tcb_buffer[old_running_buf_idx].thread_state = RUNNABLE;
 
  //Set new running thread 
  ksb->running_thread = running_buf_idx;

  //Restore status and return new context pointer
  set_svc_status(tcb_buffer[running_buf_idx].svc_state);
 // breakpoint();
  return tcb_buffer[running_buf_idx].kernel_stack_ptr;
}

/**
 * @brief	RMS scheduler implementation. 
 
 * @param[in]	curr_context_ptr	Pointer to the stack saved context fo the current thread. 

 * @return	A pointer to the stack-saved context of the next thread to be run as determined by the RMS algorithm. 
 */
void *rms(void *curr_context_ptr) { 
  
  k_threading_state_t *ksb = (k_threading_state_t *)kernel_threading_state;

  int32_t running_buf_idx = ksb->running_thread;
  uint8_t running_thread_state = tcb_buffer[running_buf_idx].thread_state;

  //Save current context
  //if(curr_context_ptr != NULL) {
    tcb_buffer[running_buf_idx].kernel_stack_ptr = curr_context_ptr;
    tcb_buffer[running_buf_idx].svc_state = get_svc_status();
  //}
  int32_t old_running_buf_idx = running_buf_idx;

  int ready_idx = 0;
  
  for(; ready_idx < MAX_U_THREADS; ready_idx++) {
    int curr_buf_idx = ksb->ready_set[ready_idx];

    if(curr_buf_idx > -1) { //Found highest priority runnable task
      //breakpoint();
      running_buf_idx = curr_buf_idx;
      break;
    }
  }
  
  
  if(ready_idx == MAX_U_THREADS) { //Special case, nothing in ready set
    uint8_t waiting = 0;  
    for(int i = 0; i < MAX_U_THREADS; i++) { //Check waiting set
      if(ksb->wait_set[i] > -1) {
        waiting = 1; 
        break;
      }
    }
    
    if(!waiting) { //Swap to default thread
      //breakpoint();
      running_buf_idx = ksb->max_threads+1;
    } else { //Swap to idle
      //breakpoint();
      running_buf_idx = ksb->max_threads;
    } 
  }

  //Remove new running task from ready set
  tcb_buffer[running_buf_idx].thread_state = RUNNING;

  //If the current thread didn't yield, add old task back to ready set
  if(running_thread_state > WAITING) //Running or runnable  
    tcb_buffer[old_running_buf_idx].thread_state = RUNNABLE;

  //Set new running thread 
  ksb->running_thread = running_buf_idx;
    
  //Restore status and return new context pointer
  set_svc_status(tcb_buffer[running_buf_idx].svc_state);
  //ASSERT(0);
  //breakpoint();
  //if(running_buf_idx !=1) breakpoint();
  return tcb_buffer[running_buf_idx].kernel_stack_ptr;
}

/**
 * @brief	PendSV interrupt handler. Runs a scheduler and then dispatches a new thread for running. 

 * @param[in]	context_ptr	A pointer to the current thread's stack-saved context. 

 * @return	A pointer to the next thread's stack-saved context. 
 */
void *pendsv_c_handler(void *context_ptr) {
  //breakpoint();
  update_kernel_sets(); //Update waiting and ready sets
  
  //Uncomment to switch to round robin scheduler implementation 
//  context_ptr = round_robin(context_ptr);
  context_ptr = rms(context_ptr);

  return context_ptr;
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

  k_threading_state_t *ksb;
  
  /* Check if proposed stack size can fit in kernel/user stack space */
  uint32_t stack_size_bytes = (1<<(mm_log2ceil_size(stack_size*WORD_SIZE)));

  uint32_t user_stack_thresh = (uint32_t)(&__thread_u_stacks_top) - (uint32_t)(&__thread_u_stacks_low);

  uint32_t kernel_stack_thresh = (uint32_t)(&__thread_k_stacks_top) - (uint32_t)(&__thread_k_stacks_low);

  uint32_t stack_consumption = (max_threads+1)*stack_size_bytes;
  
  if(stack_consumption > user_stack_thresh || stack_consumption > kernel_stack_thresh)
     return -1; 

  /* Initialize kernel data structures for threading */  
 
  ksb = (k_threading_state_t *)kernel_threading_state;

  for(int i = 0; i < BUFFER_SIZE; i++) {
    kernel_wait_set[i] = -1;
    kernel_ready_set[i] = -1;
  }

  ksb->wait_set = (signed char *)kernel_wait_set;
  ksb->ready_set = (signed char *)kernel_ready_set;

  //Default thread idx is always +1 of the maximum number of max user threads
  ksb->running_thread = max_threads+1;

  ksb->sys_tick_ct = 0;
  ksb->u_thread_ct = 0;

  ksb->stack_size = stack_size;
  ksb->max_threads = max_threads;
  ksb->max_mutexes = max_mutexes;
  ksb->mem_prot = memory_protection;

  uint32_t user_stack_brk = (uint32_t)&__thread_u_stacks_top;
  uint32_t kernel_stack_brk = (uint32_t)&__thread_k_stacks_top;
  
  /* Divide Up User & Kernel Space Stacks For User Threads */
  for(size_t i = 0; i < max_threads; i++) {
     tcb_buffer[i].user_stack_ptr = (void *)user_stack_brk;
     user_stack_brk = user_stack_brk - stack_size_bytes;
     tcb_buffer[i].kernel_stack_ptr = (void *)kernel_stack_brk;
     kernel_stack_brk = kernel_stack_brk - stack_size_bytes;
     tcb_buffer[i].thread_state = INIT;
     tcb_buffer[i].svc_state = 0;
     tcb_buffer[i].U = 0;
  }
  
  uint8_t i_thread_buf_idx = ksb->max_threads;
  uint8_t d_thread_buf_idx = ksb->max_threads+1;
  /* Set kernel state for idle thread 14 */
  tcb_buffer[i_thread_buf_idx].user_stack_ptr = (void *)user_stack_brk;
  tcb_buffer[i_thread_buf_idx].kernel_stack_ptr = (void *)kernel_stack_brk;
  tcb_buffer[i_thread_buf_idx].U = 0;
  tcb_buffer[i_thread_buf_idx].thread_state = WAITING;
  
  /* Set kernel state for default thread 15 */
  tcb_buffer[d_thread_buf_idx].thread_state = RUNNABLE;
  tcb_buffer[d_thread_buf_idx].svc_state = 0;
  tcb_buffer[d_thread_buf_idx].U = 0;
  tcb_buffer[d_thread_buf_idx].priority = D_THREAD_PRIORITY;

  /* Move idle thread to runnable*/
  if(idle_fn == NULL) {
    breakpoint();
    sys_thread_create(&default_idle, I_THREAD_PRIORITY, 0, 1, NULL);
    return 0;
  }
  breakpoint();
  sys_thread_create(idle_fn, I_THREAD_PRIORITY, 0, 1, NULL);
  return 0;
}

/**Method to kill current thread */
extern void thread_kill(void);

/**
 * @brief	System call to spawn a new thread. 

 * @return	0 on success -1 otherwise. 
 */
int sys_thread_create(
  void *fn,
  uint32_t priority,
  uint32_t C,
  uint32_t T,
  void *vargp
){
  if(!ub_test((float)T, (float)C)) return -1;
  k_threading_state_t *ksb = (k_threading_state_t *)kernel_threading_state;
  //breakpoint();
  uint8_t new_buf_idx;
  if(priority == I_THREAD_PRIORITY) { //Idle thread alloc
    //Idle thread idx is always upper bound
    new_buf_idx = ksb->max_threads;

  } else { //Normal user thread
    //new_buf_idx = ksb->u_thread_ct;
    uint8_t i = 0;
    uint8_t found_vacancy = 0;
    for(; i < ksb->max_threads; i++) {
       if(tcb_buffer[i].thread_state == INIT) {
         if(!found_vacancy) {
           new_buf_idx = i;
           found_vacancy = 1;
         }
       } else if(tcb_buffer[i].priority == priority) {
         return -1; //Non-unique priority detected
       }
       
      //if(tcb_buffer[i].priority == priority) return -1; 
    }

    //Attempted to allocate more threads than promised in init
    if(!found_vacancy) return -1;
   
   
    //if(new_buf_idx == ksb->max_threads) return -1;
  }
  
  
  uint32_t user_stack_ptr = (uint32_t)tcb_buffer[new_buf_idx].user_stack_ptr - sizeof(interrupt_stack_frame);
  
  interrupt_stack_frame *interrupt_frame = (interrupt_stack_frame *)user_stack_ptr;

  uint32_t kernel_stack_ptr = (uint32_t)tcb_buffer[new_buf_idx].kernel_stack_ptr - sizeof(thread_stack_frame);

  thread_stack_frame *thread_frame = (thread_stack_frame *)kernel_stack_ptr;
  
  //breakpoint();
  interrupt_frame->r0 = (unsigned int)vargp;
  interrupt_frame->r1 = 0;
  interrupt_frame->r2 = 0;
  interrupt_frame->r3 = 0;
  interrupt_frame->r12 = 0;
  interrupt_frame->lr = (uint32_t)&_kill;
  interrupt_frame->pc = (uint32_t)fn;
  interrupt_frame->xPSR = XPSR_INIT;

  tcb_buffer[new_buf_idx].user_stack_ptr = (void *)user_stack_ptr;
  tcb_buffer[new_buf_idx].kernel_stack_ptr = (void *)kernel_stack_ptr;
  tcb_buffer[new_buf_idx].C = C;
  tcb_buffer[new_buf_idx].T = T;
  tcb_buffer[new_buf_idx].U = (float)C/(float)T;
  tcb_buffer[new_buf_idx].thread_state = RUNNABLE;
  tcb_buffer[new_buf_idx].priority = priority;
  tcb_buffer[new_buf_idx].period_ct = 0;
  tcb_buffer[new_buf_idx].duration = 0;
  tcb_buffer[new_buf_idx].total_time = 0;
  tcb_buffer[new_buf_idx].svc_state = 0;
  
  thread_frame->psp = user_stack_ptr;
  thread_frame->r4 = 0;
  thread_frame->r5 = 0;
  thread_frame->r6 = 0;
  thread_frame->r7 = 0;
  thread_frame->r8 = 0;
  thread_frame->r9 = 0;
  thread_frame->r10 = 0;
  thread_frame->r11 = 0;
  thread_frame->r14 = LR_RETURN_TO_USER_PSP;
  
  if(priority != I_THREAD_PRIORITY) ksb->u_thread_ct++;
    
  return 0;
}

/**
 * @brief	System call to begin the rtos scheduler. Pends a PendSV so this function will only return when all previously scheduled user threads have been killed or have terminated.  

 * @param[in]	frequency	The frequency (in Hz) at which the the Systick interrupt should fire in order to re-run the scheduler to evaluate the current working pool of threads. 

 * @return	0 on success -1 on failure. 
 */
int sys_scheduler_start( uint32_t frequency ){
  uint32_t timer_period = CPU_CLK_FREQ/frequency;
  k_threading_state_t *ksb = (k_threading_state_t *)kernel_threading_state;
  ksb -> sys_tick_ct = 0;
  timer_start(timer_period);
  breakpoint();

  pend_pendsv(); //Begin first thread
  return 0;
}

/**
 * @brief	Returns priority of current running thread. 

 * @return	Priority of current running thread. 
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
  return tcb_buffer[ksb->running_thread].total_time;
}

/** 
 * @brief	Kill the currently running thread. If it is the idle thread, the default thread shall be run instead. If it is the last remaining user thread, the scheduler shall restore to the default thread. 
 */
void sys_thread_kill(){
  //breakpoint();
  k_threading_state_t *ksb = (k_threading_state_t *)kernel_threading_state;

  //Check if idle thread
  if(ksb->running_thread == ksb->max_threads) {
    //tcb_buffer[ksb->max_threads].thread_state = INIT;
    sys_thread_create(&default_idle, I_THREAD_PRIORITY, 0, 1, NULL);
    pend_pendsv();
  }

  //Check if default thread
  if(ksb->running_thread == ksb->max_threads +1) {
    sys_exit(0);
    return;
  }
  //breakpoint();
  tcb_buffer[ksb->running_thread].thread_state = INIT;
  if(ksb->running_thread != ksb->max_threads) ksb->u_thread_ct--;
  pend_pendsv();
  //wait_for_interrupt();
  //pendsv_c_handler(NULL);
  return;
}

/**
 * @brief	Have the current thread wait until the next period of execution. 
 */
void sys_wait_until_next_period(){
  k_threading_state_t *ksb = (k_threading_state_t *)kernel_threading_state;
  //if(ksb->running_thread == ksb->max_threads) 
  //  tcb_buffer[ksb->running_thread].thread_state = WAITING;
  //else 
  //breakpoint();
  tcb_buffer[ksb->running_thread].thread_state = WAITING;
  pend_pendsv();
  
  return;
}

/**
 * @brief	TODO ADD Description
 */ 
kmutex_t *sys_mutex_init( uint32_t max_prio ) {
  (void) max_prio;
  return NULL;
}

/**
 * @brief	TODO Add description
 */
void sys_mutex_lock( kmutex_t *mutex ) {
  (void) mutex;
}

/**
 * @brief	TODO Add description
 */ 
void sys_mutex_unlock( kmutex_t *mutex ) {
  (void) mutex;
}
