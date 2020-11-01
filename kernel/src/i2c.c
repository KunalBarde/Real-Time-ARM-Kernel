/**
 * @file   i2c.c
 *
 * @brief
 *
 * @date
 *
 * @author
 */
 
#include "i2c.h"

void i2c_master_init(uint16_t clk){
  (void) clk;
}

int i2c_master_start(){
  return -1;
}

int i2c_master_stop(){
  return -1;
}

int i2c_master_write( uint8_t *buf, uint16_t len, uint8_t slave_addr ){
  (void) buf; (void) len; (void) slave_addr;
  return -1;
}

int i2c_master_read(uint8_t *buf, uint16_t len, uint8_t slave_addr){
  (void) buf; (void) len; (void) slave_addr;
  return -1;
}
