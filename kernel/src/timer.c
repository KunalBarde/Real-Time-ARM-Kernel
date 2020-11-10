/**
 * @file timer.c
 *
 * @brief      STM Systick implementation. 
 *
 * @date       
 *
 * @author     Nick Toldalagi, Kunal Barde
 */

#include <timer.h>
#include <unistd.h>
#include <printk.h>
#include <gpio.h>
#include <debug.h>
#include <servok.h>

/**
* Register map for stm systick MMIO registers. 
*/
typedef struct {
  /**stk ctrl register*/
  volatile uint32_t stk_ctrl;
  /**stk load register*/
  volatile uint32_t stk_load;
  /**stk val register*/
  volatile uint32_t stk_val;
  /**stk calib register*/
  volatile uint32_t stk_calib;
} sys_tick_reg_map;

/**
*  @brief	Initialize systick timer to utilize the cpu clock and fire with specified precomputed frequency. 

*  @param	frequency	Precomputed value derived from 16MHz clock and intended sys_tick_irq firing frequency. 

*  @return	0 on success, -1 otherwise. 
*/
int timer_start(int frequency){
  
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

/**
*  @brief	Stop sys_tick timer functionality and interrupts generated by it. 
*/
void timer_stop(){  
  sys_tick_reg_map *reg_map = (sys_tick_reg_map *)SYS_TICK_BASE;

  /* Clear systick enables */
  reg_map->stk_ctrl &= ~COUNTER;
  reg_map->stk_ctrl &= ~INTERRUPT;
}


