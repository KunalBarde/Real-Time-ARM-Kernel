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
#include <gpio.h>
#include <servok.h>
#include <debug.h>
#define UNUSED __attribute__((unused))
#define SERVO_MIN_WIDTH 0.6

uint32_t servo0_tick_num = 0;
uint32_t servo1_tick_num = 0;
uint8_t servo0_en = 0;
uint8_t servo1_en = 0;

int sys_servo_enable(UNUSED uint8_t channel, UNUSED uint8_t enabled){
  if(channel > 1) return -1;
  gpio_port port = (channel == 0) ? GPIO_B : GPIO_A;
  uint32_t pin_num = (channel == 0) ? SERVO0_PIN : SERVO1_PIN;
  if(enabled) {
    if(channel == 0) servo0_en = 1;
    else servo1_en = 1;
    gpio_init(port, pin_num, MODE_GP_OUTPUT, OUTPUT_PUSH_PULL, OUTPUT_SPEED_LOW, PUPD_NONE, ALT0);
  } else {
    gpio_clr(port, pin_num);
    if (channel == 0) {
      servo0_tick_num = 0;
      servo0_en = 0;
    } else {
      servo1_tick_num = 0;
      servo1_en = 0;
    }
  } 
  return 0;
}

int sys_servo_set(UNUSED uint8_t channel, UNUSED uint8_t angle){
  if(channel == 0) {
    if(!servo0_en) return -1;
    servo0_tick_num = (uint32_t)(((float)angle/100.0 + SERVO_MIN_WIDTH)/.01);
  } else if(channel == 1) {
    if(!servo1_en) return -1;
    servo1_tick_num = (uint32_t)(((float)angle/100.0 + SERVO_MIN_WIDTH)/.01);
  } else {
    return -1;
  }
  return 0;
}
