/**
 * @file	timer.h
 *
 * @brief	Definitions for systick implementation. 
 *
 * @date       
 *
 * @author     
 */

#ifndef _TIMER_H_
#define _TIMER_H_

#define CPU_CLK_FREQ 0XF42400 /**< CPU clk frequency (16MHz) */
#define SYS_TICK_BASE 0xE000E010 /**< Systick base address */
#define COUNTER 1 /**Enable counter */
#define PROC_CLK (1 << 2) /**< Utilize proc. clk for systick*/
#define INTERRUPT (1 << 1) /**Enable inetrrupt*/

/** @brief	Start systick. */
int timer_start(int frequency);

/** @brief	Stop systick*/
void timer_stop();

#endif /* _TIMER_H_ */
