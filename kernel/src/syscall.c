/**
 * @file    syscall.c
 *
 * @brief   
 *
 * @date    
 *
 * @author  
 */

#include "syscall.h"

void *sys_sbrk( int incr ){
  (void) incr;
  return NULL;
}

int sys_write( int file, char *ptr, int len ){
  (void) file;
  (void) ptr;
  (void) len;
  return -1;
}

int sys_read( int file, char *ptr, int len ){
  (void) file;
  (void) ptr;
  (void) len;
  return -1;
}

void sys_exit( int status ){
  (void) status;
}
