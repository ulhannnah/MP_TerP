#ifndef TARGET_SELECTOR_H
#define TARGET_SELECTOR_H
#include <stdbool.h>
#include <stdint.h>
#include "target_tracker.h"

typedef struct {
    uint16_t adc_max;
    uint16_t hysteresis; /* ADC counts around current bin's boundary */
    uint8_t selected_id;
    uint8_t selectable_targets; /* 1..TRACKER_MAX_TARGETS */
} TargetSelector;

/* Legacy standalone API: all five IDs remain selectable. */
bool TargetSelector_Init(TargetSelector *s,uint16_t adc_max,uint16_t hysteresis);
/* App uses 3 by default; optionally select all 5 for expansion mode. */
bool TargetSelector_InitWithLimit(TargetSelector *s,uint16_t adc_max,
                                  uint16_t hysteresis,uint8_t limit);
uint8_t TargetSelector_Update(TargetSelector *s,uint16_t adc_value);
uint8_t TargetSelector_GetId(const TargetSelector *s);
#endif
