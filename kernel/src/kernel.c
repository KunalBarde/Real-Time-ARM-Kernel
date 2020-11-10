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

/**
* Period of the sys_tick interrupt firing. Configured to allow manual pwm control of the servo. 
*/
#define SERVO_BAUD 160

/**
* Precomputed value for configuring at 115200 baud. 
*/
#define UART_BAUD 0x8B

/**
*  @brief	Kernel main function
*/
int kernel_main( void ) {
  init_349(); // DO NOT REMOVE THIS LINE
  uart_init(UART_BAUD);
  led_driver_init();
  enter_user_mode();
  /* kernel main loop */
  return 0;
}
