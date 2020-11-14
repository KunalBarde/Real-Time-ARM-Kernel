/**
 * @file svc_handler.c
 *
 * @brief      C SVC handler for redirecting asm svc calls to c implementations of those system calls. 
 *
 * @date       11/13/2020
 *
 * @author Nick Toldalagi, Kunal Barde 
 */


#include <stdint.h>
#include <unistd.h>
#include <debug.h>
#include <printk.h>
#include <syscall.h>
#include <syscall_thread.h>
#include <svc_num.h>
#include <arm.h>

/**
* Struct representing auto-saved stack frame. Includes r0-r3, r12, lr, pc, PSR. 
*/
typedef struct {
  /**R0*/
  uint32_t r0;
  /**R1*/ 
  uint32_t r1;
  /**R2*/
  uint32_t r2;
  /**R3*/
  uint32_t r3;
  /**R12*/
  uint32_t r12; 
  /**LR*/
  uint32_t lr;
  /**program counter*/
  uint32_t pc;
  /**program status register*/
  uint32_t xPSR;
  /**5th stack-saved argument*/
  uint32_t arg5; 
} stack_frame_t;

/**
* @brief	C handler of svc calls. Will map an svc asm call to the correct c sys call. 

* @param	psp	The psp of the svc call. Will be used to access the svc instruction itself from the pc. As well as accessing for accessing arguments
*/
void svc_c_handler(void *psp) {
  stack_frame_t *s = (stack_frame_t *)psp;
  uint32_t *pc = (uint32_t *)(s -> pc -2);
  uint8_t svc_number = *(pc) & 0xFF;

  int out = 0;

  switch (svc_number) {
    case SVC_SBRK:
      out = (unsigned int)sys_sbrk(s -> r0);
      break;

    case SVC_WRITE:
      out = sys_write(s->r0, (void *)(s->r1), s->r2);
      break;

    case SVC_ISATTY:
      out = -1;
      break;

    case SVC_FSTAT:
      out = -1;
      break;

    case SVC_LSEEK:
      out = -1;
      break;

    case SVC_READ:
      out = sys_read(s->r0, (void *)(s->r1), s->r2);
      break;

    case SVC_EXIT:
      sys_exit(s->r0);
      break;

    case SVC_THR_INIT:
      out = sys_thread_init(s->r0, s->r1, (void *)s->r2, (protection_mode)s->r3, s -> arg5);
      break;

    case SVC_THR_CREATE:
      out = sys_thread_create((void *)s->r0, s->r1, s->r2, s->r3, (void *)s->arg5);
      break;

    case SVC_THR_KILL:
      sys_thread_kill();
      break;

    case SVC_SCHD_START:
      out = sys_scheduler_start(s->r0);
      break;

    case SVC_MUT_INIT:
      out = -1;
      break;

    case SVC_MUT_LOK:
      break;

    case SVC_MUT_ULK:
      break;

    case SVC_WAIT:
      sys_wait_until_next_period();
      break;

    case SVC_TIME: 
      out = sys_get_time();
      break;

    case SVC_PRIORITY:
      out = sys_get_priority();
      break;

    case SVC_THR_TIME:
      out = sys_thread_time();
      break;

    case SVC_SERVO_ENABLE:
      out = sys_servo_enable((uint8_t)s->r1, (unsigned char)s->r2);
      break;

    case SVC_SERVO_SET:
      out = sys_servo_set((unsigned char)s->r1, (unsigned char)s->r2);
      break; 

    default:
      DEBUG_PRINT( "Not implemented, svc num %d\n", svc_number );
      ASSERT( 0 );
  }
  s -> r0 = out;
}
