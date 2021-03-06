/** @file   syscall_thread.c
 *
 *  @brief	System calls used for multithreading purposes.   
 *
 *  @date	11/13/2020
 *
 *  @author	Kunal Barde, Nick Toldalagi
 */

#include <stdint.h>
#include "syscall_thread.h"
#include "syscall_mutex.h"
#include "syscall.h"
#include "mpu.h"
#include <debug.h>
#include <timer.h>
#include <arm.h>
#include <printk.h>

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

#define TCB_BUFFER_SIZE (sizeof(tcb_t) * (BUFFER_SIZE)) /**< Thread control block buffer size (in bytes)*/

#define I_THREAD_SET_IDX 14 /**<Idle thread index into kernel buffers*/
#define D_THREAD_SET_IDX 15 /**<Default thread index into kernel buffers*/

#define I_THREAD_PRIORITY 14 /**<Priority of idle thread*/
#define D_THREAD_PRIORITY 15 /**<Priority of default thread*/

#define INIT 0 /**< Initialization state for a thread */
#define WAITING 1 /**< Waiting state for a thread*/
#define RUNNABLE 2 /**< Runnable state for a thread*/
#define RUNNING 3 /**< Running state for thread*/

static volatile char blocked = 0;

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

/** @brief Global threading state */
volatile char kernel_threading_state[K_BLOCK_SIZE];

/** @brief Initially all threads should be in the wait set */
static volatile signed char kernel_wait_set[BUFFER_SIZE] = {0};

/** @brief Add threads to ready set once sys_thread_create is called */
static volatile signed char kernel_ready_set[BUFFER_SIZE] = {0};

/** @brief PendSV handler moves threads to running */
//static volatile char kernel_running_set[BUFFER_SIZE] = {0};

/** @brief Thread specific state */
static volatile tcb_t tcb_buffer[BUFFER_SIZE];

/** @brief Static global thread id assignment */
static volatile int thread_idx = 0;

/** @brief Static global mutex locked states */
static volatile uint32_t mutex_states = 0;

/** @brief Mutex specific state */
static volatile kmutex_t mutex_buffer[32];

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

  for(uint32_t i = 0; i < ksb->max_threads; i++) {
    int8_t cur_set_idx = tcb_buffer[i].priority;

    switch(tcb_buffer[i].thread_state) {
      case WAITING:
        ksb->ready_set[cur_set_idx] = -1;
        ksb->wait_set[cur_set_idx] = i;
        break;
      
      case RUNNING:
      case RUNNABLE:
        ksb->ready_set[cur_set_idx] = i;
        ksb->wait_set[cur_set_idx] = -1; 
        break;
    }
  }
}

/**
 * @brief	Responsible for the enforcement of RMS thread states. Will move threads between RUNNABLE and WAITING states as necessary. Will also handle updating bookeeping values associated with each thread. 

 * @param	curr_thread	Tcb_buffer idx of the currently running thread. 
 */
void update_thread_states(uint8_t curr_thread) {
  k_threading_state_t *ksb = (k_threading_state_t *)kernel_threading_state;
  
  //Yields upon finishing execution
  if(tcb_buffer[curr_thread].duration >= tcb_buffer[curr_thread].C) {

    if(curr_thread < ksb->max_threads) { //Only user threads can be downgraded
      tcb_buffer[curr_thread].thread_state = WAITING;
    }
  }

  //Update booking for each allocated thread
  for(uint8_t i = 0; i < ksb->max_threads; i++) {
    if(tcb_buffer[i].thread_state != INIT) {
      tcb_buffer[i].period_ct++;
      if(tcb_buffer[i].period_ct >= tcb_buffer[i].T) {
         tcb_buffer[i].period_ct = 0;
         tcb_buffer[i].duration = 0;
         tcb_buffer[i].thread_state = RUNNABLE;
      }
    }
  }
}

/**
* @brief	Handler called on occassion of sys-tick interrupt. Will call necessary functions to update thread states and then triggers a pendsv interrupt to run the scheduler. 

*/
void systick_c_handler() {
  k_threading_state_t *ksb = (k_threading_state_t *)kernel_threading_state;

  ksb->sys_tick_ct++;

  uint8_t curr_thread = ksb->running_thread;
  tcb_buffer[curr_thread].duration++;
  tcb_buffer[curr_thread].total_time++;

  update_thread_states(curr_thread);  

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

  //Safely add 1 to the current ready_set_idx so that it wraps around. (Used for checking the next thread in line). 
  running_ready_set_idx = (running_ready_set_idx >= I_THREAD_SET_IDX-1) ? 0 : running_ready_set_idx + 1;

  //Search for next valid thread in ready set
  while (1) {
    if(running_ready_set_idx == old_running_ready_set_idx) {
      //Done. We've searched all other threads so we just go back to the old thread
      return tcb_buffer[old_running_buf_idx].kernel_stack_ptr;
    }

    int curr_buf_idx = ksb -> ready_set[running_ready_set_idx];

    if(curr_buf_idx > -1) { //Found next task to run
      running_buf_idx = curr_buf_idx;
      running_ready_set_idx = tcb_buffer[running_buf_idx].priority;
      break;
    }
    
    //Safely increment to next valid index in ready set
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
  tcb_buffer[running_buf_idx].kernel_stack_ptr = curr_context_ptr;
  tcb_buffer[running_buf_idx].svc_state = get_svc_status();
  
  int32_t old_running_buf_idx = running_buf_idx;

  int ready_idx = 0;
  
  for(; ready_idx < MAX_U_THREADS; ready_idx++) {//Search for highest priority RUNNABLE task 
    int curr_buf_idx = ksb->ready_set[ready_idx];

    if(curr_buf_idx > -1) { //Found 
      running_buf_idx = curr_buf_idx;
      break;
    }
  }
  
  
  if(ready_idx == MAX_U_THREADS) { //Special case, nothing found in ready set
    uint8_t waiting = 0;  
    for(int i = 0; i < MAX_U_THREADS; i++) { //Check waiting set
      if(ksb->wait_set[i] > -1) {
        waiting = 1; 
        break;
      }
    }
    
    if(!waiting) { //Swap to default thread, nothing in waiting set
      running_buf_idx = ksb->max_threads+1;
    } else { //Swap to idle, tasks in waiting set
      running_buf_idx = ksb->max_threads;
    } 
  }

  if(blocked) {
    blocked = 0;
    running_buf_idx = find_highest_locker();
  }

  //Remove new running task from ready set
  tcb_buffer[running_buf_idx].thread_state = RUNNING;

  //If the current thread didn't yield (was just RUNNING or RUNNABLE), add old task back to ready set
  if(running_thread_state > WAITING) 
    tcb_buffer[old_running_buf_idx].thread_state = RUNNABLE;

  //Set new running thread 
  ksb->running_thread = running_buf_idx;
    
  //Restore status and return new context pointer
  set_svc_status(tcb_buffer[running_buf_idx].svc_state);

  protection_mode prot_mode = ksb->mem_prot;

  void *user_stack_ptr = tcb_buffer[ksb->running_thread].user_stack_ptr;
  void *kernel_stack_ptr = tcb_buffer[ksb->running_thread].kernel_stack_ptr;

  if(prot_mode == KERNEL_ONLY) {
    mm_enable_user_stacks(user_stack_ptr, kernel_stack_ptr, -1);
  } else {
    mm_disable_user_stacks();
    mm_enable_user_stacks(user_stack_ptr, kernel_stack_ptr, ksb->running_thread);
  }

  return tcb_buffer[running_buf_idx].kernel_stack_ptr;
}

/**
 * @brief	PCP scheduler implementation. 
 
 * @param[in]	curr_context_ptr	Pointer to the stack saved context fo the current thread. 

 * @return	A pointer to the stack-saved context of the next thread to be run as determined by the PCP algorithm. 
 */
void *pcp(void *curr_context_ptr) { 
  
  k_threading_state_t *ksb = (k_threading_state_t *)kernel_threading_state;

  int32_t running_buf_idx = ksb->running_thread;
  uint8_t running_thread_state = tcb_buffer[running_buf_idx].thread_state;

  //Save current context
  tcb_buffer[running_buf_idx].kernel_stack_ptr = curr_context_ptr;
  tcb_buffer[running_buf_idx].svc_state = get_svc_status();
  
  int32_t old_running_buf_idx = running_buf_idx;

  int ready_idx = 0;
  
  for(; ready_idx < MAX_U_THREADS; ready_idx++) {//Search for highest priority RUNNABLE task 
    int curr_buf_idx = ksb->ready_set[ready_idx];

    if(curr_buf_idx > -1) { //Found 
      running_buf_idx = curr_buf_idx;
      break;
    }
  }
  
  
  if(ready_idx == MAX_U_THREADS) { //Special case, nothing found in ready set
    uint8_t waiting = 0;  
    for(int i = 0; i < MAX_U_THREADS; i++) { //Check waiting set
      if(ksb->wait_set[i] > -1) {
        waiting = 1; 
        break;
      }
    }
    
    if(!waiting) { //Swap to default thread, nothing in waiting set
      running_buf_idx = ksb->max_threads+1;
    } else { //Swap to idle, tasks in waiting set
      running_buf_idx = ksb->max_threads;
    } 
  }

  if((uint32_t)running_buf_idx >= (uint32_t)ksb->priority_ceiling && tcb_buffer[running_buf_idx].blocked) {
    running_buf_idx = find_highest_locker();
  }

  //Remove new running task from ready set
  tcb_buffer[running_buf_idx].thread_state = RUNNING;

  //If the current thread didn't yield (was just RUNNING or RUNNABLE), add old task back to ready set
  if(running_thread_state > WAITING) 
    tcb_buffer[old_running_buf_idx].thread_state = RUNNABLE;

  //Set new running thread 
  ksb->running_thread = running_buf_idx;
    
  //Restore status and return new context pointer
  set_svc_status(tcb_buffer[running_buf_idx].svc_state);

  protection_mode prot_mode = ksb->mem_prot;

  void *user_stack_ptr = tcb_buffer[ksb->running_thread].user_stack_ptr;
  void *kernel_stack_ptr = tcb_buffer[ksb->running_thread].kernel_stack_ptr;

  if(prot_mode == KERNEL_ONLY) {
    mm_enable_user_stacks(user_stack_ptr, kernel_stack_ptr, -1);
  } else {
    mm_disable_user_stacks();
    mm_enable_user_stacks(user_stack_ptr, kernel_stack_ptr, ksb->running_thread);
  }

  tcb_buffer[running_buf_idx].blocked = 0;
  return tcb_buffer[running_buf_idx].kernel_stack_ptr;
}

/**
 * @brief	PendSV interrupt handler. Runs a scheduler and then dispatches a new thread for running. 

 * @param[in]	context_ptr	A pointer to the current thread's stack-saved context. 

 * @return	A pointer to the next thread's stack-saved context. 
 */
void *pendsv_c_handler(void *context_ptr) {

  update_kernel_sets(); //Update waiting and ready sets

  //context_ptr = rms(context_ptr);
  context_ptr = pcp(context_ptr);
  return context_ptr;
}

/** 
 * @brief	System call to initialize a new thread. 
 
 * @param[in]	max_threads	Maximum number of user threads. Cannot be great than 14. 
 * @param[in]	stack_size	Stack size for each thread in bytes. 
 * @param[in]	idle_fn	Idle function to be used by scheduler. If NULL a default one shall be utilized. 
 * @param[in]	memory_protection	UNUSED
 * @param[in]	max_mutexes	UNUSED

 * @return	0 on success -1 otherwise. 
 */
int sys_thread_init(
  uint32_t max_threads,
  uint32_t stack_size,
  void *idle_fn,
  protection_mode memory_protection,
  uint32_t max_mutexes
){
  
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
  ksb->u_mutex_ct = 0;
  ksb->priority_ceiling = -1;

  ksb->stack_size = stack_size_bytes;
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
     tcb_buffer[i].blocked = 0;
  }

  ksb->thread_u_stacks_bottom = (void *)&__thread_u_stacks_low;
  ksb->thread_k_stacks_bottom = (void *)&__thread_k_stacks_low;
  
  //idle thread is always next thread after last user thread in tcb_buffer. Default follows. 
  uint8_t i_thread_buf_idx = ksb->max_threads;
  uint8_t d_thread_buf_idx = ksb->max_threads+1;

  /* Set kernel state for idle thread 14 */
  tcb_buffer[i_thread_buf_idx].user_stack_ptr = (void *)user_stack_brk;
  tcb_buffer[i_thread_buf_idx].kernel_stack_ptr = (void *)kernel_stack_brk;
  tcb_buffer[i_thread_buf_idx].U = 0;
  tcb_buffer[i_thread_buf_idx].thread_state = WAITING;
  tcb_buffer[i_thread_buf_idx].blocked = 0;
  

  /* Set kernel state for default thread 15 */
  tcb_buffer[d_thread_buf_idx].thread_state = RUNNABLE;
  tcb_buffer[d_thread_buf_idx].svc_state = 0;
  tcb_buffer[d_thread_buf_idx].U = 0;
  tcb_buffer[d_thread_buf_idx].priority = D_THREAD_PRIORITY;
  tcb_buffer[d_thread_buf_idx].inherited_prior = D_THREAD_PRIORITY;
  tcb_buffer[d_thread_buf_idx].blocked = 0;

  /* Move idle thread to runnable*/
  if(idle_fn == NULL) {
    sys_thread_create(&default_idle, I_THREAD_PRIORITY, 0, 1, NULL);
    return 0;
  }

  sys_thread_create(idle_fn, I_THREAD_PRIORITY, 0, 1, NULL);
  return 0;
}

/**Method to kill current thread */
extern void thread_kill(void);

/**
 * @brief	System call to spawn a new thread. Schedulability verified using UB test. 
 
 * @param[in]	fn	Function to be executed by new thread. 
 * @param[in]	priority	Priority of new thread. 
 * @param[in]	C	Worst case computation time of new thread. 
 * @param[in]	T	Period of new thread. 
 * @param[in]	vargp	Argument to thread function. 

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

  uint8_t new_buf_idx;
  if(priority == I_THREAD_PRIORITY) { //Idle thread alloc

    new_buf_idx = ksb->max_threads;

  } else { //Normal user thread

    uint8_t i = 0;
    uint8_t found_vacancy = 0;
    for(; i < ksb->max_threads; i++) {//Search for next vacant spot in tcb_buffer for new thread. 
       if(tcb_buffer[i].thread_state == INIT) {
         if(!found_vacancy) {
           new_buf_idx = i;
           found_vacancy = 1;
         }
       } else if(tcb_buffer[i].priority == priority) {
         return -1; //Non-unique priority detected
       }
    }

    //Attempted to allocate more threads than promised in init
    if(!found_vacancy) return -1;
  }

  uint32_t user_stack_brk = (uint32_t)&__thread_u_stacks_top;
  uint32_t kernel_stack_brk = (uint32_t)&__thread_k_stacks_top;
  
  uint32_t stack_size = ksb->stack_size;

  tcb_buffer[new_buf_idx].kernel_stack_ptr = (void *)(kernel_stack_brk - new_buf_idx*stack_size);
  tcb_buffer[new_buf_idx].user_stack_ptr = (void *)(user_stack_brk - new_buf_idx*stack_size);
  
  uint32_t user_stack_ptr = (uint32_t)tcb_buffer[new_buf_idx].user_stack_ptr - sizeof(interrupt_stack_frame);
  interrupt_stack_frame *interrupt_frame = (interrupt_stack_frame *)user_stack_ptr;

  uint32_t kernel_stack_ptr = (uint32_t)tcb_buffer[new_buf_idx].kernel_stack_ptr - sizeof(thread_stack_frame);
  thread_stack_frame *thread_frame = (thread_stack_frame *)kernel_stack_ptr;
  
  //Initialize user stack frame
  interrupt_frame->r0 = (unsigned int)vargp;
  interrupt_frame->r1 = 0;
  interrupt_frame->r2 = 0;
  interrupt_frame->r3 = 0;
  interrupt_frame->r12 = 0;
  interrupt_frame->lr = (uint32_t)&_kill;
  interrupt_frame->pc = (uint32_t)fn;
  interrupt_frame->xPSR = XPSR_INIT;

  //Initialize tcb_buffer entry for new thread
  tcb_buffer[new_buf_idx].user_stack_ptr = (void *)user_stack_ptr;
  tcb_buffer[new_buf_idx].kernel_stack_ptr = (void *)kernel_stack_ptr;
  tcb_buffer[new_buf_idx].C = C;
  tcb_buffer[new_buf_idx].T = T;
  tcb_buffer[new_buf_idx].U = (float)C/(float)T;
  tcb_buffer[new_buf_idx].thread_state = RUNNABLE;
  tcb_buffer[new_buf_idx].priority = priority;
  tcb_buffer[new_buf_idx].inherited_prior = priority;
  tcb_buffer[new_buf_idx].period_ct = 0;
  tcb_buffer[new_buf_idx].duration = 0;
  tcb_buffer[new_buf_idx].total_time = 0;
  tcb_buffer[new_buf_idx].svc_state = 0;
  
  //Initialize kernel stack frame
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

  //Only count new user threads in count
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
  if(timer_start(timer_period)) return -1;

  pend_pendsv(); //Begin first thread
  return 0;
}

/**
 * @brief	Returns priority of current running thread. 

 * @return	Priority of current running thread. 
 */
uint32_t sys_get_priority(){
  k_threading_state_t *ksb = (k_threading_state_t *)kernel_threading_state;

  return tcb_buffer[ksb->running_thread].inherited_prior;
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
  k_threading_state_t *ksb = (k_threading_state_t *)kernel_threading_state;

  //Check if idle thread
  if(ksb->running_thread == ksb->max_threads) {
    //Swap to default idle thread fn
    sys_thread_create(&default_idle, I_THREAD_PRIORITY, 0, 1, NULL);
    pend_pendsv();
  }

  //Check if default thread
  if(ksb->running_thread == ksb->max_threads+1) {
    sys_exit(0);
    return;
  }

  tcb_buffer[ksb->running_thread].thread_state = INIT;

  int8_t priority = tcb_buffer[ksb->running_thread].priority;
  ksb->ready_set[priority] = -1;
  ksb->wait_set[priority]=-1;
  if(ksb->running_thread != ksb->max_threads) ksb->u_thread_ct--;
  pend_pendsv();

  return;
}

/**
 * @brief	Have the current thread wait until the next period of execution. 
 */
void sys_wait_until_next_period(){
  k_threading_state_t *ksb = (k_threading_state_t *)kernel_threading_state;
  if(!check_no_locks(ksb->running_thread))
    DEBUG_PRINT( "Warning, thread yielding while holding resources.\n" );

  tcb_buffer[ksb->running_thread].thread_state = WAITING;
  pend_pendsv();
  
  return;
}

/**
 * @brief	Initialize a mutex. 

 * @param[in]	max_prio	The maximum priority of this mutex. 0 is the highest priority. Will not be checked for validity until thread attempts to lock the mutex. 

 * @return	A pointer to the newly created mutex struct. 
 */ 
kmutex_t *sys_mutex_init( uint32_t max_prio ) {
  k_threading_state_t *ksb = (k_threading_state_t *)kernel_threading_state;
 
  uint32_t free_mutex;
  if((free_mutex = ksb->u_mutex_ct) >= ksb->max_mutexes)
    return NULL;
  
  mutex_buffer[free_mutex].max_prior = max_prio;
  mutex_buffer[free_mutex].mutex_num = free_mutex;
  ksb->u_mutex_ct++;
  return (kmutex_t *)&(mutex_buffer[free_mutex]);
}

/**
 * @brief	Lock mutex. Will hang until the mutex has been acquired. 

 * @param[in]	mutex	Mutex to be acquired. 
 */
void sys_mutex_lock( kmutex_t *mutex ) {
  
  k_threading_state_t *ksb = (k_threading_state_t *)kernel_threading_state;

  if(ksb->running_thread == ksb->max_threads) 
    DEBUG_PRINT( "Idle thread attempting to lock mutex \n" );
    //printk( "Idle thread attempting to lock mutex \n" );
  
  uint32_t mutex_num = mutex -> mutex_num;
  uint32_t curr_ceil = tcb_buffer[ksb->running_thread].priority;
  if(mutex->max_prior > curr_ceil) {
    DEBUG_PRINT( "Warning! Thread attempted to lock mutex with insufficient ceiling. Killing thread...\n" );

    sys_thread_kill();
  }

  //Locked and being locked by same thread
  if(mutex->locked_by == ksb->running_thread && (mutex_states & (0x1 << mutex_num))) {
    DEBUG_PRINT( "Warning! Attempted to lock previously locked mutex.\n" );
    return;
  }

  int32_t priority_ceiling = ksb->priority_ceiling; 
  uint32_t max_prior = mutex->max_prior;
  //Attempt to lock
  if((uint32_t)priority_ceiling <= curr_ceil) {
    //Check if nested
    if(ksb->running_thread == find_highest_locker()) {
      mutex_states |= 0x1 << mutex_num;
      mutex->locked_by = ksb->running_thread;
      ksb->priority_ceiling = (max_prior < (uint32_t)priority_ceiling) ? (int32_t)mutex->max_prior : priority_ceiling;
      return;
    }
    raise_blocking_priority(curr_ceil);
  } else {
    //Lock
    mutex_states |= 0x1 << mutex_num;
    mutex->locked_by = ksb->running_thread;
    ksb->priority_ceiling = (max_prior < (uint32_t)priority_ceiling) ? (int32_t)mutex->max_prior : priority_ceiling;
    return;
  }

  //Wait to acquire
  while(!acquire_mutex(curr_ceil, mutex->max_prior, mutex_num)) {
    tcb_buffer[ksb->running_thread].blocked = 1;
    blocked = 1;
    pend_pendsv();
  }
}

/**
 * @brief	Helper polling function to check if a thread is able to acquire a specific mutex given its current priority.

 * @param[in]	curr_ceil	Current static ceiling of acquiring thread.
 * @param[in]	max_prior	Max_prior of the the thread to be acquired.
 * @param[in]	mutex_num	Mutex id of the mutex to be acquired.

 * @return	1 on success. 0 otherwise. 
 */
int acquire_mutex(uint32_t curr_ceil, uint32_t max_prior, uint8_t mutex_num) {
  kmutex_t *mutex = (kmutex_t *)&mutex_buffer[mutex_num];
  k_threading_state_t *ksb = (k_threading_state_t *)kernel_threading_state;
  if((uint32_t)ksb->priority_ceiling > curr_ceil) {
    mutex_states |= 0x1 << mutex_num;
    mutex->locked_by = ksb->running_thread;
    ksb->priority_ceiling = max_prior;
    return 1;
  }

  return 0;
}

/**
 * @brief	Raises blocking thread's inherited priority to match at least current blocked thread's.
 */
void raise_blocking_priority(uint32_t curr_ceil) {
  int32_t blocking_thread_idx = find_highest_locker();

  //Check if update needed 
  if(tcb_buffer[blocking_thread_idx].inherited_prior > curr_ceil)
    tcb_buffer[blocking_thread_idx].inherited_prior = curr_ceil;
}

/**
 * @brief	Find max_prior of locked resource with the highest priority; 

 * @return	-1 if no locked mutexes found. Else it is max_prior of the locked mutex with the highest max_prior
 */
int32_t find_highest_locked() {
  int32_t highest = -1;

  for(int i = 0; i < 32; i++) {
    if(mutex_states & (0x1 << i)) {
      if ((uint32_t)highest > mutex_buffer[i].max_prior)
        highest = mutex_buffer[i].max_prior;
    }
  }

  return highest;
}

/**
 * @brief	Find thread ID of thread blocking the resource with the highest priority; 

 * @return	-1 if no locking thread found. Else it is the thread ID of the thread blocking the mutex with the largest max_priority
 */
int32_t find_highest_locker() {
  int32_t highest = -1;
  int32_t highest_ind = -1;

  for(int i = 0; i < 32; i++) {
    if(mutex_states & (0x1 << i)) {
      if ((uint32_t)highest > mutex_buffer[i].max_prior) {
        highest = mutex_buffer[i].max_prior;
        highest_ind = mutex_buffer[i].locked_by;
      }
    }
  }

  return highest_ind;
}

/**
 * @brief	Check that thread with given thread id does not currently hold any locks. 

 * @param[in]	thread_buf_idx	Thread id of thread to be checked.  

 * @return	1 if no locks. 0 if it does hold a lock.
 */
int check_no_locks(uint32_t thread_buf_idx) {

  for(int i = 0; i < 32; i++) {
    if(mutex_states & (0x1 << i)) {
      if(mutex_buffer[i].locked_by == thread_buf_idx)
        return 0;
    }
  }
  return 1;
}

/**
 * @brief	Unlock specific mutex. Will hang until it has been sucessfully unlocked.
 
 * @param[in]	mutex	Mutex to be unlocked. 
 */ 
void sys_mutex_unlock( kmutex_t *mutex ) {
  uint32_t mutex_num = mutex->mutex_num;
  uint32_t locked_by = mutex->locked_by;

  //Unlocked and being 
  if(!(mutex_states & (0x1 << mutex_num))) {
    DEBUG_PRINT( "Warning! Attempted to unlock previously unlocked mutex.\n");
    return;
  }
  
  k_threading_state_t *ksb = (k_threading_state_t *)kernel_threading_state;
  
  //Check unlock by another mutex
  if(ksb->running_thread != mutex->locked_by) {
    blocked = 1;
    pend_pendsv();
  }

  //Unlock
  mutex_states &= ~(0x1 << mutex_num);

  //Update priority ceiling and inherited priority
  ksb->priority_ceiling = find_highest_locked();

  tcb_buffer[locked_by].inherited_prior = tcb_buffer[locked_by].priority;
  pend_pendsv();

}
