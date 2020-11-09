/**
 * @file 
 *
 * @brief      
 *
 * @date       
 *
 * @author     
 */

#ifndef _TIMER_H_
#define _TIMER_H_
#define SYS_TICK_BASE 0xE000E010
#define COUNTER 1
#define PROC_CLK (1 << 2)
#define INTERRUPT (1 << 1)

int timer_start(int frequency);

void timer_stop();

#endif /* _TIMER_H_ */
