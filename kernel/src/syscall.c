/**

 * @file 
 *
 * @brief      
 *
 * @date       
 *
 * @author     
 */

#include <unistd.h>
#include <syscall.h>
#include <printk.h>
#include <uart.h>
#include <led_driver.h>
#include <kmalloc.h>
#include <arm.h>
#include <debug.h>

#define UNUSED __attribute__((unused))
extern char __heap_low[];
extern char __heap_top[];

static char *heap_brk = 0;

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

int sys_write(UNUSED int file, UNUSED char *ptr, UNUSED int len){
  if(file == 1) {
    int bytes = 0;
    for(int i = 0; i < len; i++) {
       if(uart_put_byte(ptr[i]) == 0) {
          bytes+=1;
       }
    }
    //ASSERT(1 == 0);
    //uart_flush();
    return bytes;
  }else{
    //Invalid file descriptor
    return -1;
  }
}

int sys_read(UNUSED int file, UNUSED char *ptr, UNUSED int len){
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

void sys_exit(UNUSED int status){
  led_set_display(status);
  printk("%d\n", status);
  uart_flush();
  disable_interrupts();
  wait_for_interrupt();
}
