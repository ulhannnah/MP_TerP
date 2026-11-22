#ifndef DSP_FIR_H
#define DSP_FIR_H
#include <stdbool.h>
#include <stdint.h>
#define DSP_FIR_MAX_TAPS 32u
typedef struct {
    int16_t coeff[DSP_FIR_MAX_TAPS];
    int16_t history[DSP_FIR_MAX_TAPS];
    uint8_t taps;
    uint8_t newest;
} DSPFIR;
bool DSPFIR_Init(DSPFIR *f,const int16_t *coeff,uint8_t taps);
void DSPFIR_Reset(DSPFIR *f);
int16_t DSPFIR_ProcessReference(DSPFIR *f,int16_t sample);
int16_t DSPFIR_ProcessPaired(DSPFIR *f,int16_t sample);
/* For Cortex-M4F with CMSIS DSP intrinsics, compile with
 * MP_TERP_ENABLE_ARM_DSP_INTRINSICS to select __SMLALD pair MAC.
 * Other targets use the portable two-MAC fallback.
 * Compare identical coefficient sets in SEPARATE FIR instances.
 */
#endif
