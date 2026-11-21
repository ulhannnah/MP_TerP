#include <assert.h>
#include <stdio.h>
#include "servo.h"

FTM_Type fake_ftms[4];
PORT_Type fake_port;
PCC_Type fake_pcc;

static ServoConfig config(FTM_Type *ftm, uint8_t ch)
{
    ServoConfig c;
    c.ftm=ftm; c.channel=ch; c.port=&fake_port;
    c.port_pcc_index=PCC_PORTA_INDEX;
    c.pin=1u; c.pin_alt=2u; c.clock_pcs=6u; c.clock_hz=40000000u;
    c.period_us=20000u; c.minimum_pulse_us=1000u; c.maximum_pulse_us=2000u;
    c.minimum_angle_mdeg=0; c.maximum_angle_mdeg=180000;
    c.initial_angle_mdeg=90000;
    return c;
}

int main(void)
{
    Servo a,b,c;
    ServoConfig cfg=config(FTM1,5u);
    int32_t angle=0;
    assert(Servo_Init(NULL,&cfg)==SERVO_ERR_ARGUMENT);
    assert(Servo_Init(&a,NULL)==SERVO_ERR_ARGUMENT);
    assert(Servo_Init(&a,&cfg)==SERVO_OK);
    assert(a.prescaler==4u);
    assert(FTM1->MOD==49999u);
    assert(FTM1->CONTROLS[5].CnV==3750u);
    assert(FTM1->CONTROLS[5].CnSC==(FTM_CnSC_MSB_MASK|FTM_CnSC_ELSB_MASK));
    assert(FTM1->SC==(FTM_SC_PS(4)|FTM_SC_CLKS(1)|(FTM_SC_PWMEN0_MASK<<5)));
    assert(fake_port.PCR[1]==PORT_PCR_MUX(2));
    assert((PCC->PCCn[PCC_FTM1_INDEX]&PCC_PCCn_PCS_MASK)==PCC_PCCn_PCS(6));
    assert(Servo_Init(&b,&cfg)==SERVO_ERR_BUSY);
    assert(Servo_SetAngle(&a,0)==SERVO_OK);
    assert(FTM1->CONTROLS[5].CnV==2500u);
    assert(Servo_SetAngle(&a,180000)==SERVO_OK);
    assert(FTM1->CONTROLS[5].CnV==5000u);
    assert(Servo_SetAngle(&a,180001)==SERVO_ERR_RANGE);
    assert(Servo_SetPulseUs(&a,1500u)==SERVO_OK);
    assert(Servo_GetAngle(&a,&angle)==SERVO_OK && angle==90000);
    assert(Servo_SetPulseUs(&a,2600u)==SERVO_ERR_RANGE);
    assert(Servo_Stop(&a)==SERVO_OK && FTM1->SC==0u);
    assert(Servo_Start(&a)==SERVO_OK && a.running);
    assert(Servo_Deinit(&a)==SERVO_OK);
    assert(Servo_Init(&b,&cfg)==SERVO_OK);
    assert(Servo_Deinit(&b)==SERVO_OK);

    cfg=config(FTM3,1u); cfg.clock_pcs=1u; cfg.clock_hz=8000000u;
    assert(Servo_Init(&c,&cfg)==SERVO_OK);
    assert(c.prescaler==2u && FTM3->MOD==39999u);
    assert(Servo_Deinit(&c)==SERVO_OK);

    cfg=config(FTM0,0u); cfg.channel=8u;
    assert(Servo_Init(&c,&cfg)==SERVO_ERR_ARGUMENT);
    cfg=config(FTM0,0u); cfg.maximum_pulse_us=22000u;
    assert(Servo_Init(&c,&cfg)==SERVO_ERR_ARGUMENT);
    cfg=config(FTM0,0u); cfg.clock_hz=100000000u; cfg.period_us=10000000u;
    assert(Servo_Init(&c,&cfg)==SERVO_ERR_CLOCK);
    puts("PASS: FTM/channel, prescaler, pulse mapping, limits, resource ownership");
    return 0;
}
