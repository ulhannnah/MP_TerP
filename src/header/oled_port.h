#ifndef OLED_PORT_H_
#define OLED_PORT_H_

#include <stdint.h>

/* Port layer: change only these functions when moving to another MCU/bus. */
void OLED_PortInit(void);
/* 7-bit I2C address, SSD1306 control byte (0x00 command / 0x40 data).
 * Return 0 on success or a negative error code on failure.
 */
int OLED_PortWrite(uint8_t address7, uint8_t control,
                   const uint8_t *data, uint16_t len);

#endif /* OLED_PORT_H_ */
