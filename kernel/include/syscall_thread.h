/** @file syscall_thread.h

 *
 *  @brief  Custom syscalls to support thread library.
 *
 *  @date   March 27, 2019
 *
 *  @author Ronit Banerjee <ronitb@andrew.cmu.edu>
 */

#ifndef _SYSCALL_THREAD_H_
#define _SYSCALL_THREAD_H_

#include <unistd.h>

/**
 * @enum protection_mode
 *
 * @brief      Enums for protection mode, PER_THREAD and KERNEL_ONLY.
 */
typedef enum { PER_THREAD = 1, KERNEL_ONLY = 0 } protection_mode;


/**
 * @struct	Thread control block struct. 	
 */
typedef struct {
  void *user_stack_ptr; /**< User stack pointer, probably not needed  */
  void *kernel_stack_ptr; /**< Kernel stack pointer. Points to context of the thread on the thread's stack*/
  uint32_t priority; /**< Thread static priority*/
  uint32_t inherited_prior;
  uint32_t C; /**< Thread worst case runtime. */
  uint32_t T; /**< Thread execution period*/
  uint32_t duration; /**< Current execution elapsed time. */
  uint32_t total_time; /**< Total cpu time consumed over all executions. */
  uint32_t period_ct; /**< Number of ticks into current period.*/
  float U; /**< Thread utilization.*/
  int svc_state; /**< Thread svc state. */
  uint8_t thread_state; /**< Thread current state. */
}tcb_t;

/**
 * @struct	Struct representing a kernel stack-saved thread context 
 */
typedef struct {
  uint32_t psp; /**< @brief Register value for psp */
  uint32_t r4; /**< @brief Register value for r4 */
  uint32_t r5; /**< @brief Register value for r5 */
  uint32_t r6; /**< @brief Register value for r6 */
  uint32_t r7; /**< @brief Register value for r7 */
  uint32_t r8; /**< @brief Register value for r8 */
  uint32_t r9; /**< @brief Register value for r9 */
  uint32_t r10; /**< @brief Register value for r10 */
  uint32_t r11; /**< @brief Register value for r11 */
  uint32_t r14; /**< @brief Register value for r14 (lr) */
}thread_stack_frame;

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

/**
 * @struct	Struct representing current threading state of the kernel. 
 */
typedef struct {
  signed char *wait_set; /**< Priority ordered mapping of threads which are waiting to their tcb's. 0 is highest priority. Must be disjoint with the ready set.*/
  signed char *ready_set; /**< Priority ordered mapping of threads which are ready for execution to their tcb's. 0 is highest priority. Must be disjoint with the waiting set. */
  uint8_t running_thread; /**< Tbuf index of currently running thread*/
  uint32_t sys_tick_ct; /**< Used for time slicing and scheduling*/
  uint32_t stack_size; /**< Stack size per thread*/
  uint32_t u_thread_ct; /**< Number of currently allocated user threads */
  uint32_t max_threads; /**< Maximum number of allocatable user threads. Determined by user at thread initialization */
  uint32_t max_mutexes;
  uint32_t u_mutex_ct;
  void *thread_u_stacks_bottom;
  void *thread_k_stacks_bottom;
  protection_mode mem_prot;
  int32_t priority_ceiling;
}k_threading_state_t;

#define K_BLOCK_SIZE (sizeof(k_threading_state_t)) /**< sizeof(k_thread_state_t)*/

extern volatile char kernel_threading_state[K_BLOCK_SIZE];

/**
 * @brief      The SysTick interrupt handler.
 */
void systick_c_handler( void );

/**
 * @brief      The PendSV interrupt handler.
 */
void *pendsv_c_handler( void * );

/**
 * @brief      Initialize the thread library
 *
 *             A user program must call this initializer before attempting to
 *             create any threads or starting the scheduler.
 *
 * @param[in]  max_threads        Maximum number of threads that will be
 *                                created.
 * @param[in]  stack_size         Declares the size in words of all user and
 *                                kernel stacks created.
 * @param[in]  idle_fn            Pointer to a thread function to run when no
 *                                other threads are runnable. If NULL is
 *                                is supplied, the kernel will provide its
 *                                own idle function that will sleep.
 * @param[in]  memory_protection  Enum for memory protection, either
 *                                PER_THREAD or KERNEL_ONLY
 * @param[in]  max_mutexes        Maximum number of mutexes that will be
 *                                created.
 *
 * @return     0 on success or -1 on failure
 */
int sys_thread_init(
  uint32_t        max_threads,
  uint32_t        stack_size,
  void           *idle_fn,
  protection_mode memory_protection,
  uint32_t        max_mutexes
);

/**
 * @brief      Create a new thread running the given function. The thread will
 *             not be created if the UB test fails, and in that case this function
 *             will return an error.
 *
 * @param[in]  fn     Pointer to the function to run in the new thread.
 * @param[in]  prio   Priority of this thread. Lower number are higher
 *                    priority.
 * @param[in]  C      Real time execution time (scheduler ticks).
 * @param[in]  T      Real time task period (scheduler ticks).
 * @param[in]  vargp  Argument for thread function (usually a pointer).
 *
 * @return     0 on success or -1 on failure
 */
int sys_thread_create( void *fn, uint32_t prio, uint32_t C, uint32_t T, void *vargp );

/**
 * @brief      Allow the kernel to start running the thread set.
 *
 *             This function should enable SysTick and thus enable your
 *             scheduler. It will not return immediately unless there is an error.
 *			   It may eventually return successfully if all thread functions are
 *   		   completed or killed.
 *
 * @param[in]  frequency  Frequency (Hz) of context swaps.
 *
 * @return     0 on success or -1 on failure
 */
int sys_scheduler_start( uint32_t frequency );

/**
 * @brief      Get the current time.
 *
 * @return     The time in ticks.
 */
uint32_t sys_get_time( void );

/**
 * @brief      Get the effective priority of the current running thread
 *
 * @return     The thread's effective priority
 */
uint32_t sys_get_priority( void );

/**
 * @brief      Gets the total elapsed time for the thread (since its first
 *             ever period).
 *
 * @return     The time in ticks.
 */
uint32_t sys_thread_time( void );

/**
 * @brief      Waits efficiently by descheduling thread.
 */
void sys_wait_until_next_period( void );

/**
* @brief      Kills current running thread. Aborts program if current thread is
*             main thread or the idle thread or if current thread exited
*             while holding a mutex.
*/
void sys_thread_kill( void );

int check_no_locks(uint32_t n);

void raise_blocking_priority(uint32_t curr_ceil);

int acquire_mutex(uint32_t curr_ceil, uint32_t max_prior, uint8_t mutex_num);

int32_t find_highest_locker();

#endif /* _SYSCALL_THREAD_H_ */
