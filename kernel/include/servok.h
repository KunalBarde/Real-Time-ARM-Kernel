/**
 * @file 
 *
 * @brief
 *
 * @date
 *
 * @author
 */

#ifndef _SERVOK_H_
#define _SERVOK_H_

#define SERVO0_PIN 10
#define SERVO1_PIN 8

int sys_servo_enable(uint8_t channel, uint8_t enabled);

int sys_servo_set(uint8_t channel, uint8_t angle);

extern uint32_t servo0_tick_num;
extern uint32_t servo1_tick_num;
extern uint8_t servo0_en;
extern uint8_t servo1_en;

#endif /* _SERVOK_H_ */
