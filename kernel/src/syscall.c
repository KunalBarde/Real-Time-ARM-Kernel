/**

 * @file syscall.c
 *
 * @brief      Implementation of kernel space syscalls. 
 *
 * @date       11/3/2020
 *
 * @author     Nick Toldalagi, Kunal Barde
 */

#include <unistd.h>
#include <syscall.h>
#include <printk.h>
#include <uart.h>
#include <led_driver.h>
#include <kmalloc.h>
#include <arm.h>
#include <debug.h>

/** Used for designating non-implemented portions of code for the compiler. */
#define UNUSED __attribute__((unused))

/** Bottom of user heap */
extern char __heap_low[];

/** Top of user heap */
extern char __heap_top[];

/** Current heap brk */
static char *heap_brk = 0;

/**
* @brief	sbrk system call implementation. Attempts to increase available heap size by an increment. 

* @param	incr	Increment of bytes by which to increase the heap. 

* @return	(void*)-1 if the heap cannot be extended by the requested increment, otherwise a void pointer to the newly designated memory of size incr.  
*/
void *sys_sbrk(UNUSED int incr){
  char *tmp;
  if(!heap_brk) {
     heap_brk = __heap_low;
  }

  tmp = heap_brk;
  if(heap_brk + (size_t)incr >= __heap_top) {
     return (void *)-1;
  }
  
  heap_brk = heap_brk + (size_t)incr;
  return (void *)tmp; 
}

/**
* @brief	Implementation of sys call write. Maps to user calls of write. 

* @param	file	File pointer to write to. Currently only 1, (stdout) is accepted. 
* @param	ptr	String which should be written. 
* @param	len	Number of bytes which should be written. 

* @return	-1 on failure, otherwise the number of byte sucessfully written to stdout. 
*/
int sys_write(UNUSED int file, UNUSED char *ptr, UNUSED int len){
  if(file == 1) {
    int state = save_interrupt_state_and_disable();
    for(int i = 0; i < len; i++) {
       while(uart_put_byte(ptr[i]));
    }
    restore_interrupt_state(state);
    return len;
  }else{
    //Invalid file descriptor
    return -1;
  }
}

/**
* @brief	Implementation of system call read. Maps to user call of read(). 

* @param	file	File from which to read. Currently only able to read from STDIN (0). 
* @param	ptr	Pointer to buffer where bytes will be read to. 
* @param	len	Number of bytes to read into ptr buffer. 

* @return	-1 on failure, otherwise the number of bytes read into the buffer from stdin. This may be <= len. 
*/
int sys_read(int file, char *ptr, int len){
  if(file != 0) return -1;
  char c;
  int count = 0;
  while(count < len) {
     if(!uart_get_byte(&c)) {
        if(c == '\n' || c == '\r') {
           uart_put_byte('\n');
           ptr[count] = '\n';
           count++;
           return count;
        }else if(c == '\b'){
           if(count > 0) count--;
           uart_put_byte('\b');
           uart_put_byte(' ');
        }else if(c == EOT) {
           return count;
        }
        uart_put_byte(c);
        *(ptr+count) = c;
        count++;
     }
  }
  return count;
}

/**
* @brief	Implementation of system exit. Will display exit status on the led display, write status to stdout, and flush the uart before sleeping indefinitely. 

* @param	status	Exit status. 0 indicates normal (no error).
*/
void sys_exit(int status){
  led_set_display(status);
  printk("%d\n", status);
  uart_flush();
  disable_interrupts();
  wait_for_interrupt();
}
