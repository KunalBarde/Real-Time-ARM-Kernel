/** @file    mpu.h
 *
 *  @brief
 *
 *  @date
 *
 *  @author
 */
#ifndef _MPU_H_
#define _MPU_H_

#include <unistd.h>

/**
 * @brief  Returns ceiling (log_2 n).
 */
uint32_t mm_log2ceil_size(uint32_t n);

void mm_enable_mpu(int enable, int background);

int mm_enable_user_access();

int mm_enable_user_stacks(void *process_stack, void *kernel_stack, int thread_num);

void mm_disable_user_access();

void mm_disable_user_stacks();

void mm_region_disable(uint32_t region_number);

int mm_region_enable(uint32_t region_number, void *base_address, uint8_t size_log2, int execute, int user_write_access);

#endif /* _MPU_H_ */
