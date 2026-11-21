#ifndef TARGET_SELECTOR_H
#define TARGET_SELECTOR_H
#include <stdbool.h>
#include <stdint.h>
typedef struct {
    uint16_t adc_max;
    uint16_t hysteresis; /* ADC counts around current bin's boundary */
    uint8_t selected_id;
} TargetSelector;
bool TargetSelector_Init(TargetSelector *s,uint16_t adc_max,uint16_t hysteresis);
uint8_t TargetSelector_Update(TargetSelector *s,uint16_t adc_value);
uint8_t TargetSelector_GetId(const TargetSelector *s);
#endif
