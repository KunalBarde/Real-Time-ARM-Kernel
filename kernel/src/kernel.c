/**
 * @file kernel.c
 *
 * @brief      Kernel entry point
 *
 * @date       
 * @author     Nick Toldalagi, Kunal Barde
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
#include <mpu.h>

/**
* Period of the sys_tick interrupt firing. Configured to allow manual pwm control of the servo. 
*/
#define SERVO_BAUD 160

/**
*  @brief	Kernel main function. Initializes necessary hardware driver libraries and then breaks to user mode. 
*/
int kernel_main( void ) {
  init_349(); // DO NOT REMOVE THIS LINE
  uart_init(USART_DIV);
  led_driver_init();
  mm_enable_mpu(1);
  mm_enable_user_access();
  enter_user_mode();
  mm_disable_user_access();
  return 0;
}
