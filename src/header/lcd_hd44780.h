#ifndef LCD_HD44780_H
#define LCD_HD44780_H
#include <stdbool.h>
#include <stdint.h>
#include "device_registers.h"
typedef void (*LCDDelayUs)(void *context,uint32_t microseconds);
typedef struct {
    PORT_Type *port;
    GPIO_Type *gpio;
    uint32_t port_pcc_index;
    uint8_t pin;
} LCDPin;
typedef struct {
    LCDPin rs,enable,data[4]; /* data[0..3] = D4..D7, RW tied LOW */
    LCDDelayUs delay_us;
    void *delay_context;
    uint8_t columns;
    uint8_t rows; /* 1 or 2 */
} LCDHD44780Config;
typedef struct {
    LCDHD44780Config config;
    uint8_t initialized;
} LCDHD44780;
bool LCDHD44780_Init(LCDHD44780 *lcd,const LCDHD44780Config *cfg);
bool LCDHD44780_WriteLine(void *context,uint8_t row,const char *text);
void LCDHD44780_Clear(LCDHD44780 *lcd);
#endif
