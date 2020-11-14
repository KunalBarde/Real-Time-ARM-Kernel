/**
 * @file	syscall.h
 *
 * @brief	Definitions for supported system calls.      
 *
 * @date       
 *
 * @author     
 */

#ifndef _SYSCALLS_H_
#define _SYSCALLS_H_
#define EOT 4

/** @brief	Mapped to sbrk() sys call*/
void *sys_sbrk(int incr);

/** @brief	Mapped to write() sys call*/
int sys_write(int file, char *ptr, int len);

/** @brief	Mapped to read() sys call*/
int sys_read(int file, char *ptr, int len);

/** @brief	Mapped to exit() sys call*/
void sys_exit(int status);

/** @brief	Mapped to servo_enable() sys call*/
int sys_servo_enable(uint8_t channel, uint8_t enabled);

/** @brief	Mapped to servo_set() sys call*/
int sys_servo_set(uint8_t channel, uint8_t angle);

#endif /* _SYSCALLS_H_ */
