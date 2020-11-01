/**
 * @file
 *
 * @brief
 *
 * @date
 *
 * @author
 */

#include "led_driver.h"

uint8_t hex_to_seven_segment(uint8_t hex);

void led_driver_init(uint32_t addr) {
  (void) addr;
}


void led_set_display(uint32_t input){
  (void) input;
}

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
