/**
 * @file    svc_handler.c
 *
 * @brief
 *
 * @date 
 *
 * @author
 */

#include <stdint.h>
#include <debug.h>
#include <svc_num.h>

typedef struct _interrupt_stack_frame {
  uint32_t r0;
  uint32_t r1;
  uint32_t r2;
  uint32_t r3;
  uint32_t r12;
  uint32_t lr;
  uint32_t pc;
  uint32_t xpsr;
  uint32_t arg4;
} interrupt_stack_frame;

void svc_c_handler( /* TODO: fill in args */ ) {
  uint8_t svc_num = -1;
  switch ( svc_num ) {
    default:
      DEBUG_PRINT( "Not implemented, svc num %d\n", svc_num );
      ASSERT( 0 );
  }
}
