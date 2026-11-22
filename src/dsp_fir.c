#include "dsp_fir.h"
#include <stddef.h>
#if defined(MP_TERP_ENABLE_ARM_DSP_INTRINSICS)
#include "device_registers.h"
#endif
static uint8_t history_index(const DSPFIR *f,uint8_t offset){
    return (uint8_t)((f->newest+f->taps-offset)%f->taps);
}
static int16_t saturate_q15(int64_t accum){
    int64_t val=accum>=0?(accum+16384)/32768:-((-accum+16384)/32768);
    if(val>32767)return 32767;
    if(val<(-32768))return -32768;
    return (int16_t)val;
}
bool DSPFIR_Init(DSPFIR *f,const int16_t *coeff,uint8_t taps){
    if(!f||!coeff||taps==0u||taps>DSP_FIR_MAX_TAPS)return false;
    f->taps=taps;
    for(uint8_t i=0u;i<taps;i++)f->coeff[i]=coeff[i];
    DSPFIR_Reset(f);return true;
}
void DSPFIR_Reset(DSPFIR *f){
    if(!f||f->taps==0u||f->taps>DSP_FIR_MAX_TAPS)return;
    f->newest=0u;
    for(uint8_t i=0u;i<f->taps;i++)f->history[i]=0;
}
static void push(DSPFIR *f,int16_t sample){
    f->newest=(uint8_t)((f->newest+1u)%f->taps);
    f->history[f->newest]=sample;
}
int16_t DSPFIR_ProcessReference(DSPFIR *f,int16_t sample){
    int64_t acc=0;
    if(!f||f->taps==0u)return 0;
    push(f,sample);
    for(uint8_t i=0u;i<f->taps;i++)
        acc+=(int32_t)f->history[history_index(f,i)]*(int32_t)f->coeff[i];
    return saturate_q15(acc);
}
int16_t DSPFIR_ProcessPaired(DSPFIR *f,int16_t sample){
    int64_t acc=0;
    if(!f||f->taps==0u)return 0;
    push(f,sample);
    uint8_t i=0u;
    for(;i+1u<f->taps;i=(uint8_t)(i+2u)){
        int16_t x0=f->history[history_index(f,i)];
        int16_t x1=f->history[history_index(f,(uint8_t)(i+1u))];
        int16_t c0=f->coeff[i],c1=f->coeff[i+1u];
#if defined(MP_TERP_ENABLE_ARM_DSP_INTRINSICS)
        int32_t packed_x=(int32_t)((uint32_t)(uint16_t)x0|((uint32_t)(uint16_t)x1<<16));
        int32_t packed_c=(int32_t)((uint32_t)(uint16_t)c0|((uint32_t)(uint16_t)c1<<16));
        acc=__SMLALD(packed_x,packed_c,acc);
#else
        acc+=(int32_t)x0*(int32_t)c0+(int64_t)(int32_t)x1*(int32_t)c1;
#endif
    }
    if(i<f->taps)
        acc+=(int32_t)f->history[history_index(f,i)]*(int32_t)f->coeff[i];
    return saturate_q15(acc);
}
