#include "adc_channel.h"
#include <stddef.h>
bool ADCChannel_Init(ADCChannel *a,const ADCChannelConfig *cfg){
    if(!a||!cfg||!(cfg->adc==ADC0||cfg->adc==ADC1)||
       cfg->clock_pcs==0u||cfg->clock_pcs>7u||cfg->channel>31u||
       cfg->vref_mv==0u)return false;
    a->initialized=0u;a->config=*cfg;
    PCC->PCCn[cfg->pcc_index]&=~PCC_PCCn_CGC_MASK;
    PCC->PCCn[cfg->pcc_index]=(PCC->PCCn[cfg->pcc_index]&~PCC_PCCn_PCS_MASK)|
                               PCC_PCCn_PCS(cfg->clock_pcs)|PCC_PCCn_CGC_MASK;
    cfg->adc->SC1[0]=ADC_SC1_ADCH(31);
    cfg->adc->CFG1=ADC_CFG1_ADIV(0)|ADC_CFG1_MODE(1);
    cfg->adc->CFG2=ADC_CFG2_SMPLTS(12);
    cfg->adc->SC2=0u;cfg->adc->SC3=0u;
    a->initialized=1u;return true;
}
bool ADCChannel_Start(ADCChannel *a){
    if(!a||!a->initialized)return false;
    a->config.adc->SC1[0]=ADC_SC1_ADCH(a->config.channel);
    return true;
}
bool ADCChannel_Poll(ADCChannel *a,uint16_t *raw){
    if(!a||!a->initialized||!raw)return false;
    if((a->config.adc->SC1[0]&ADC_SC1_COCO_MASK)==0u)return false;
    *raw=(uint16_t)(a->config.adc->R[0]&0x0FFFu);return true;
}
uint32_t ADCChannel_ToMillivolts(const ADCChannel *a,uint16_t raw){
    if(!a||!a->initialized)return 0u;
    return ((uint32_t)raw*a->config.vref_mv+2047u)/4095u;
}
