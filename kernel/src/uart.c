/**
 * @file   uart.c
 *
 * @brief
 *
 * @date
 *
 * @author
 */
 
#include "uart.h"

void uart_init( int baud ){
  (void) baud;
}

int uart_put_byte( char c ){
  (void) c;
  return -1;
}

int uart_get_byte( char *c ){
  (void) c;
  return -1;
}

void uart_irq_handler(){
}

void uart_flush(){
}
