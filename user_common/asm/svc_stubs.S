/**
 * @file   swi_stubs.S
 *
 * @brief  Stub functions for newlib and 349libc syscalls
 *
 * @date
 * @author
 */

.cpu cortex-m4
.syntax unified
.section .swi_stub
.thumb

#include "../../kernel/include/svc_num.h"

.global _start
_start:
  ;bkpt
  SVC SVC_SCHD_START
  bkpt

.global _sbrk
_sbrk:
  SVC SVC_SBRK
  bx lr
  bkpt

.global _write
_write:
  SVC SVC_WRITE
  bx lr
  bkpt

.global _close
_close:
  SVC SVC_CLOSE
  bkpt

.global _fstat
_fstat:
  SVC SVC_FSTAT
  bx lr
  bkpt

.global _isatty
_isatty:
  SVC SVC_ISATTY
  bx lr
  bkpt

.global _lseek
_lseek:
  SVC SVC_LSEEK
  bx lr
  bkpt

.global _read
_read:
  SVC SVC_READ
  bx lr
  bkpt

.global _gettimeofday
_gettimeofday:
  SVC SVC_TIME
  bkpt

.global _times
_times:
  SVC SVC_TIME
  bkpt

.global _getpid
_getpid:
  bkpt

.type _kill, %function
.global _kill
_kill:
  SVC SVC_THR_KILL
  
  bx lr
  bkpt

.global _exit
_exit:
  SVC SVC_EXIT
  bkpt

.type thread_init, %function 
.global thread_init 
thread_init:
  SVC SVC_THR_INIT
  bx lr
  bkpt 

.type thread_create, %function 
.global thread_create 
thread_create:
  SVC SVC_THR_CREATE
  bx lr
  bkpt 

.type scheduler_start, %function 
.global scheduler_start 
scheduler_start:
  SVC SVC_SCHD_START
  bx lr
  bkpt 

.type mutex_init, %function 
.global mutex_init 
mutex_init:
  SVC SVC_MUT_INIT
  bx lr
  bkpt 

.type mutex_lock, %function 
.global mutex_lock
mutex_lock:
  SVC SVC_MUT_LOK
  bx lr
  bkpt 

.type mutex_unlock, %function 
.global mutex_unlock 
mutex_unlock:
  SVC SVC_MUT_ULK
  bx lr
  bkpt 

.type wait_until_next_period, %function 
.global wait_until_next_period 
wait_until_next_period:
  SVC SVC_WAIT
  bx lr
  bkpt 

.type default_idle, %function
.global default_idle
default_idle:
  
  wfi
  b default_idle
  bkpt

.global get_time 
get_time:
  SVC SVC_TIME
  bx lr
  bkpt 

.global get_priority
get_priority:
  SVC SVC_PRIORITY
  bx lr
  bkpt

.global thread_time 
thread_time:
  SVC SVC_THR_TIME
  bx lr
  bkpt 

.global servo_enable
servo_enable:
  SVC SVC_SERVO_ENABLE
  bx lr
  bkpt

.global servo_set
servo_set:
  SVC SVC_SERVO_SET
  bx lr
  bkpt

