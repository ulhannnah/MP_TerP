#ifndef GPIO_IO_H
#define GPIO_IO_H
#include <stdbool.h>
#include <stdint.h>
#include "device_registers.h"
typedef struct {
    PORT_Type *port;
    GPIO_Type *gpio;
    uint32_t port_pcc_index;
    uint8_t pin;
    bool active_high;
} GpioOutputConfig;
typedef struct {GpioOutputConfig config;uint8_t initialized;} GpioOutput;
bool GpioOutput_Init(GpioOutput *out,const GpioOutputConfig *cfg);
void GpioOutput_Write(GpioOutput *out,bool active);

typedef struct {
    PORT_Type *port;
    GPIO_Type *gpio;
    uint32_t port_pcc_index;
    uint8_t pin;
    bool active_low;
    bool internal_pullup;
} GpioButtonConfig;
typedef struct {
    GpioButtonConfig config;
    volatile uint8_t pending;
    uint8_t initialized;
} GpioButton;
/* PORTx_IRQHandler must call GpioButton_OnPortIrq; app enables NVIC. */
bool GpioButton_Init(GpioButton *btn,const GpioButtonConfig *cfg);
void GpioButton_OnPortIrq(GpioButton *btn);
bool GpioButton_TakePress(GpioButton *btn);
#endif
