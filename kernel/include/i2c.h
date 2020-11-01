/**
 * @file    i2c.h
 *
 * @brief   
 *
 * @date
 *
 * @author
 */

#ifndef _I2C_H_
#define _I2C_H_

#include <unistd.h>

/* Clock control register value for 100kHz in Standard Mode */
#define I2C_CCR_100KHZ 40

void i2c_master_init(uint16_t clk);

int i2c_master_start();

int i2c_master_stop();

int i2c_master_write(uint8_t *buf, uint16_t len, uint8_t slave_addr);

int i2c_master_read(uint8_t *buf, uint16_t len, uint8_t addr);

#endif /* _I2C_H_ */
