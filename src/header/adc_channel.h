#ifndef ADC_CHANNEL_H
#define ADC_CHANNEL_H
#include <stdbool.h>
#include <stdint.h>
#include "device_registers.h"
typedef struct {
    ADC_Type *adc;
    uint32_t pcc_index;
    uint8_t clock_pcs;
    uint8_t channel;
    uint32_t vref_mv;
} ADCChannelConfig;
typedef struct {
    ADCChannelConfig config;
    uint8_t initialized;
} ADCChannel;
bool ADCChannel_Init(ADCChannel *a,const ADCChannelConfig *config);
bool ADCChannel_Start(ADCChannel *a);
bool ADCChannel_Poll(ADCChannel *a,uint16_t *raw);
uint32_t ADCChannel_ToMillivolts(const ADCChannel *a,uint16_t raw);
#endif
