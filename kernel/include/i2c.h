// James Zhang
#ifndef _I2C_H_
#define _I2C_H_
#define I2C1_BASE  (struct i2c_reg_map *) 0x40005400

#define APBCLK_I2C_EN (1 << 21)
#define APBCLK_FREQ (0x10) //16 Mhz

#define I2CCLK_FREQ (0x50) 
#define I2C_EN (0x1)
#define I2C_START (1 << 8)
#define I2C_STOP (1 << 9)
#define I2C_TRISE (0x11)
#define I2C_ACK (1 << 10)

#define LWR_12BITS (0XFFF)
#define LWR_5BITS (0x1F)
#define LWR_8BITS (0xFF)

#define SR_SB (0x1)
#define SR_MSL (0x1)
#define SR_ADDR (0x2)
#define SR_TxE (1 << 7)
#define SR_BTF (1 << 2)
#define SR_RxNE (1 << 6)

#include <stdint.h>

void i2c_master_init(uint16_t clk);

void i2c_master_start();

void i2c_master_stop();

int i2c_master_write(uint8_t *buf, uint16_t len, uint8_t slave_addr);

int i2c_master_read(uint8_t *buf, uint16_t len, uint8_t slave_addr);

#endif /* _I2C_H_ */
