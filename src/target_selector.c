#include "target_selector.h"
#include <stddef.h>

bool TargetSelector_InitWithLimit(TargetSelector *s,uint16_t max,
                                  uint16_t h,uint8_t limit)
{
    if(!s || limit==0u || limit>TRACKER_MAX_TARGETS ||
       max<limit || h>=max/10u) return false;
    s->adc_max=max;
    s->hysteresis=h;
    s->selectable_targets=limit;
    s->selected_id=1u;
    return true;
}

bool TargetSelector_Init(TargetSelector *s,uint16_t max,uint16_t h)
{
    return TargetSelector_InitWithLimit(s,max,h,TRACKER_MAX_TARGETS);
}

uint8_t TargetSelector_Update(TargetSelector *s,uint16_t adc)
{
    uint32_t current,low,high,value,count;
    if(!s || s->selectable_targets==0u ||
       s->selectable_targets>TRACKER_MAX_TARGETS ||
       s->selected_id<1u || s->selected_id>s->selectable_targets) return 0u;

    count=s->selectable_targets;
    value=adc>s->adc_max?s->adc_max:adc;
    current=s->selected_id;
    low=(current-1u)*((uint32_t)s->adc_max+1u)/count;
    high=current*((uint32_t)s->adc_max+1u)/count;
    if(value+s->hysteresis>=low &&
       (current==count || value<high+s->hysteresis)) return s->selected_id;
    s->selected_id=(uint8_t)((value*count)/((uint32_t)s->adc_max+1u)+1u);
    if(s->selected_id>count)s->selected_id=(uint8_t)count;
    return s->selected_id;
}

uint8_t TargetSelector_GetId(const TargetSelector *s)
{
    return s ? s->selected_id : 0u;
}
