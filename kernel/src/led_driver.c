#include <i2c.h>
#include <led_driver.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

uint8_t hex_to_seven_segment(uint8_t hex);

/** @brief Initialize led driver ic and initialize i2c as necessary.*/
void led_driver_init(){
  i2c_master_init(APBCLK_FREQ_CONFIG);
  
  uint8_t buf[17] = {0};
  buf[0] = CMD_SYSSETUP;
 
  //System setup and turn on oscillator
  i2c_master_start();
  i2c_master_write(buf, 1, HTK_ADDR);
  i2c_master_stop();

  buf[0] = CMD_DISSETUP;
  
  //Display setup
  i2c_master_start();
  i2c_master_write(buf, 1, HTK_ADDR);
  i2c_master_stop();
  
  buf[0] = CMD_FULLDIM;
 
  //Set to full brightness
  i2c_master_start();
  i2c_master_write(buf, 1, HTK_ADDR);
  i2c_master_stop();
 
  buf[0] = 0;
 
  //Clear RAM
  i2c_master_start();
  i2c_master_write(buf, 17, HTK_ADDR);
  i2c_master_stop();  
 
  return;
}

/** @brief Set led display to a 16B hex value.*/
void led_set_display(uint32_t input){
  uint8_t buf[2] = {0};

  buf[0] = 0x08;
  buf[1] = hex_to_seven_segment(input & 0xF);
  input = input >> 4;

  i2c_master_start();
  i2c_master_write(buf, 2, HTK_ADDR);
  i2c_master_stop();

  buf[0] = 0x06;
  buf[1] = hex_to_seven_segment(input & 0xF);
  input = input >> 4;


  i2c_master_start();
  i2c_master_write(buf, 2, HTK_ADDR);
  i2c_master_stop();
  
  buf[0] = 0x02;
  buf[1] = hex_to_seven_segment(input & 0xF);
  input = input >> 4;


  i2c_master_start();
  i2c_master_write(buf, 2, HTK_ADDR);
  i2c_master_stop();

  buf[0] = 0x00;
  buf[1] = hex_to_seven_segment(input & 0xF);

  i2c_master_start();
  i2c_master_write(buf, 2, HTK_ADDR);
  i2c_master_stop();

  return;
}

/** @brief Converts a hex value into the correct byte value to write to the led ic.
    @param hex Value to be converted. */
uint8_t hex_to_seven_segment(uint8_t hex){
  uint8_t result;
  switch (hex){
    case 0x0:
      result = 0b00111111;
      break;
    case 0x1:
      result = 0b00000110;
      break;
    case 0x2:
      result = 0b01011011;
      break;
    case 0x3:
      result = 0b01001111;
      break;
    case 0x4:
      result = 0b01100110;
      break;
    case 0x5:
      result = 0b01101101;
      break;
    case 0x6:
      result = 0b01111101;
      break;
    case 0x7:
      result = 0b00000111;
      break;
    case 0x8:
      result = 0b01111111;
      break;
    case 0x9:
      result = 0b01101111;
      break;
    case 0xA:
      result = 0b01110111;
      break;
    case 0xB:
      result = 0b01111100;
      break;
    case 0xC:
      result = 0b00111001;
      break;
    case 0xD:
      result = 0b01011110;
      break;
    case 0xE:
      result = 0b01111001;
      break;
    case 0xF:
      result = 0b01110001;
      break;
    default:
      result = 0;
  }
  return result;
}
