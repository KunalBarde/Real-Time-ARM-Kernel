/**
 * @file 
 *
 * @brief      
 *
 * @date       
 *
 * @author     
 */

#include <timer.h>
#include <unistd.h>
#include <printk.h>
#include <gpio.h>
#include <debug.h>
#include <servok.h>

#define UNUSED __attribute__((unused))

typedef struct {
  volatile uint32_t stk_ctrl;
  volatile uint32_t stk_load;
  volatile uint32_t stk_val;
  volatile uint32_t stk_calib;
} sys_tick_reg_map;

volatile uint32_t sys_ticks = 0;

int timer_start(UNUSED int frequency){
  
  sys_tick_reg_map *reg_map = (sys_tick_reg_map *)SYS_TICK_BASE;
  
  /* Set reload value as specified by specific frequency */ 
  reg_map->stk_load |= frequency;

  /* Enable SysTick counter & interrupt */
  reg_map->stk_ctrl |= PROC_CLK;

  /* Enable interrupts from the counter block */
  reg_map->stk_ctrl |= COUNTER;

  /* Enable sys tick counter */
  reg_map->stk_ctrl |= INTERRUPT;

  return 0;
}

void timer_stop(){  
  sys_tick_reg_map *reg_map = (sys_tick_reg_map *)SYS_TICK_BASE;

  /* Clear systick enables */
  reg_map->stk_ctrl &= ~COUNTER;
  reg_map->stk_ctrl &= ~INTERRUPT;
}

