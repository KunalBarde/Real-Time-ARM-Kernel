/**
 * @file 
 *
 * @brief      
 *
 * @date       
 *
 * @author     
 */

#include <stdint.h>
#include <unistd.h>
#include <debug.h>
#include <printk.h>
#include <syscall.h>
#include <svc_num.h>

#define UNUSED __attribute__((unused))

typedef struct {
uint32_t r0;
uint32_t r1;
uint32_t r2;
uint32_t r3;
uint32_t r12;
uint32_t lr;
uint32_t pc;
uint32_t xPSR;

} stack_frame_t;

void svc_c_handler(void *psp, int arg1, int arg2) {
  stack_frame_t *s = (stack_frame_t *)psp;
  uint32_t *pc = (uint32_t *)(s -> pc -2);
  uint8_t svc_number = *(pc) & 0xFF;
  //int svc_number = -1;
  //printk("SVC Number %x\n", svc_number);
  //svc_number = svc_number & 0xFF;
  //while(1);
  int out;
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
    case SVC_SERVO_ENABLE:
      out = sys_servo_enable((uint8_t)arg1, (unsigned char)arg2);
      break;
    case SVC_SERVO_SET:
      out = sys_servo_set((unsigned char)arg1, (unsigned char)arg2);
      break; 
    default:
      DEBUG_PRINT( "Not implemented, svc num %d\n", svc_number );
      ASSERT( 0 );
  }
  s -> r0 = out;
}
