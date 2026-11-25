#ifndef LCD_HD44780_H
#define LCD_HD44780_H
#include <stdbool.h>
#include <stdint.h>
#include "device_registers.h"

#define LCDHD44780_MAX_COLS 20u

typedef void (*LCDDelayUs)(void *context,uint32_t microseconds);
typedef struct {
    PORT_Type *port;
    GPIO_Type *gpio;
    uint32_t port_pcc_index;
    uint8_t pin;
} LCDPin;
typedef struct {
    LCDPin rs,enable,data[4]; /* data[0..3] = D4..D7, RW tied LOW */
    LCDDelayUs delay_us;      /* used for blocking POWER-ON initialization */
    void *delay_context;
    uint8_t columns;
    uint8_t rows; /* 1 or 2 */
} LCDHD44780Config;

/* One foreground owner. IRQ code must never call the LCD functions.
 * The asynchronous API uses the 1ms system tick to space each of the
 * 4 nibble edges; it performs no delay/busy loop during runtime.
 * LCDHD44780_WriteLine remains available for existing bring-up code. */
typedef struct {
    LCDHD44780Config config;
    uint8_t initialized;
    char desired[2][LCDHD44780_MAX_COLS + 1u];
    char displayed[2][LCDHD44780_MAX_COLS + 1u];
    char active_line[LCDHD44780_MAX_COLS + 1u];
    uint32_t next_action_ms;
    uint32_t service_steps;
    uint32_t completed_lines;
    uint8_t tx_active;
    uint8_t service_started;
    uint8_t active_row;
    uint8_t byte_index; /* 0: DDRAM address, 1..columns: characters */
    uint8_t phase;      /* 0: high E+, 1: high E-, 2: low E+, 3: low E- */
} LCDHD44780;

bool LCDHD44780_Init(LCDHD44780 *lcd,const LCDHD44780Config *cfg);

/* Legacy synchronous writes, for independent hardware bring-up only. */
bool LCDHD44780_WriteLine(void *context,uint8_t row,const char *text);
void LCDHD44780_Clear(LCDHD44780 *lcd);

/* Stage 5: copy a new line to RAM without touching GPIO; latest wins.
 * Queueing while transmitting keeps the in-flight snapshot intact. */
bool LCDHD44780_QueueLine(void *context,uint8_t row,const char *text);

/* Advance at most one GPIO edge step per tick. No delay_us calls, no loops
 * waiting for LCD, no timer channel allocated. Returns true if an edge step
 * executed. E stays high across approximately 1ms between Service calls;
 * this deliberately conservative timing must still be checked on hardware. */
bool LCDHD44780_Service(LCDHD44780 *lcd,uint32_t now_ms);

/* Reports whether desired display differs from completed transmit state. */
bool LCDHD44780_IsBusy(const LCDHD44780 *lcd);

#endif
