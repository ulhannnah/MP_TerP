#include "target_selector.h"
#include <stddef.h>
bool TargetSelector_Init(TargetSelector *s,uint16_t max,uint16_t h){
    if(!s||max<5u||h>=max/10u)return false;
    s->adc_max=max;s->hysteresis=h;s->selected_id=1u;return true;
}
uint8_t TargetSelector_Update(TargetSelector *s,uint16_t adc){
    uint32_t current, low,high,value;
    if(!s||s->selected_id<1u||s->selected_id>5u)return 0u;
    value=adc>s->adc_max?s->adc_max:adc;
    current=s->selected_id;
    low=(uint32_t)(current-1u)*((uint32_t)s->adc_max+1u)/5u;
    high=current*((uint32_t)s->adc_max+1u)/5u;
    if(value+s->hysteresis>=low &&
       (current==5u||value<high+s->hysteresis))return s->selected_id;
    s->selected_id=(uint8_t)((value*5u)/((uint32_t)s->adc_max+1u)+1u);
    if(s->selected_id>5u)s->selected_id=5u;
    return s->selected_id;
}
uint8_t TargetSelector_GetId(const TargetSelector *s){return s?s->selected_id:0u;}
