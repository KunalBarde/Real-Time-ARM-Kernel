/**
 * @file kernel.c
 *
 * @brief      Kernel entry point
 *
 * @date       
 * @author     
 */

#include "arm.h"
#include "kernel.h"
#include <gpio.h>
#include <timer.h>
#include <printk.h>
#include <uart.h>
#include <unistd.h>
#include <stdio.h>
#include <led_driver.h>
#include <servok.h>
#define SERVO_BAUD 160
#define UART_BAUD 0x8B

int kernel_main( void ) {
  init_349(); // DO NOT REMOVE THIS LINE
  uart_init(UART_BAUD);
  led_driver_init();
  timer_start(SERVO_BAUD);
  enter_user_mode();
  /* kernel main loop */
  return 0;
}
