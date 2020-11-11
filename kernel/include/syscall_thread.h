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
  uint32_t user_stack_ptr; /**< User stack pointer, probably not needed  */
  uint32_t kernel_stack_ptr; /**< Kernel stack pointer. Points to context of the thread on the thread's stack*/
  uint32_t priority;
  uint32_t C;
  uint32_t T;
  uint32_t duration;
  uint32_t period_ct;
  float U;
  int svc_state;
  uint8_t thread_state;
}tcb_t;

/** 
 * @struct	Struct representing stack-saved thread context. 
 */ 
/*typedef struct {
  uint32_t r14; < LR 
  uint32_t r11;
  uint32_t r10;
  uint32_t r9;
  uint32_t r8;
  uint32_t r7;
  uint32_t r6;
  uint32_t r5;
  uint32_t r4;
  uint32_t psp;
}thread_stack_frame;*/


typedef struct {
  uint32_t psp;
  uint32_t r4;
  uint32_t r5;
  uint32_t r6;
  uint32_t r7;
  uint32_t r8;
  uint32_t r9;
  uint32_t r10;
  uint32_t r11;
  uint32_t r14;
}thread_stack_frame;

/**
 * @struct	Struct representing current threading state of the kernel. 
 */
typedef struct {
  uint8_t *wait_set; /**< Priority ordered mapping of threads which are waiting to their tcb's. 0 is highest priority. Must be disjoint with the ready set.*/
  uint8_t *ready_set; /**< Priority ordered mapping of threads which are ready for execution to their tcb's. 0 is highest priority. Must be disjoint with the waiting set. */
  uint8_t running_thread; /**< Tbuf index of currently running thread*/
  uint32_t sys_tick_ct; /**< Used for time slicing and scheduling*/
  uint32_t stack_size; /**< Stack size per thread*/
  uint32_t u_thread_ct; /**< Number of currently allocated user threads */
  uint32_t max_threads; /**< Maximum number of allocatable user threads. Determined by user at thread initialization */
  uint32_t max_mutexes;
  protection_mode mem_prot;
}k_threading_state_t;

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
*
* @return     Does not return.
*/
void sys_thread_kill( void );

#endif /* _SYSCALL_THREAD_H_ */
