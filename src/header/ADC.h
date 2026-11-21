
#ifndef ADC_H_
#define ADC_H_

#include <stdint.h>
#include "device_registers.h"

/*
 * ADC reference voltage in millivolts.
 * Verify against the actual board reference.
 */
#ifndef ADC_VREF_MV
#define ADC_VREF_MV (5000u)
#endif

void ADC_init(void);

void ADC_init_HWTrigger(char Channel);

void convertAdcChan(uint16_t adcChan);

uint8_t adc_complete(void);

/* Raw 12-bit ADC result: 0..4095 */
uint16_t read_adc_raw(void);

/* Result converted to millivolts */
uint32_t read_adc_chx(void);

#endif /* ADC_H_ */
