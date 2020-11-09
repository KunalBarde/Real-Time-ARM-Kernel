// James Zhang
#ifndef _LED_DRIVER_H_
#define _LED_DRIVER_H_

#define HTK_ADDR 0xE0

#define CMD_SYSSETUP 0x21
#define CMD_DISSETUP 0x81
#define CMD_FULLDIM 0xEF

#define APBCLK_FREQ_CONFIG (0x50) //16 Mhz

#define slv_read 1

#include <unistd.h>

void led_driver_init();
void led_set_display(uint32_t input);

#endif /* _LED_DRIVER_H_ */
