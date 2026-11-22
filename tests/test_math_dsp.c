#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "dsp_fir.h"
#include "coordinate_math.h"
int main(void){
    const int16_t coeff[]={8192,8192,8192,8192};
    DSPFIR ref,opt;
    assert(DSPFIR_Init(&ref,coeff,4u));
    assert(DSPFIR_Init(&opt,coeff,4u));
    for(int i=0;i<100;i++){
        int16_t input=(int16_t)((i%7-3)*5000);
        assert(DSPFIR_ProcessReference(&ref,input)==DSPFIR_ProcessPaired(&opt,input));
    }
    Coordinate2D p,q,r;
    Velocity2D v;
    assert(Coordinate_PolarToCartesian(0,1000.0f,&p));
    assert(p.x_mm>999.9f&&p.y_mm<0.1f);
    assert(Coordinate_PolarToCartesian(90000,1000.0f,&q));
    assert(q.y_mm>999.9f&&q.x_mm<0.1f);
    assert(Coordinate_CalculateVelocity(&p,&q,1000u,&v));
    assert(Coordinate_Predict(&p,&v,1000u,&r));
    assert(r.y_mm>999.9f&&r.x_mm<0.1f);
    puts("PASS: FIR Q15 reference/paired equivalence, FPU coordinate math");
    return 0;
}
