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
    assert(FTM1->SC==(FTM_SC_PS(4)|FTM_SC_CLKS(3)|(FTM_SC_PWMEN0_MASK<<5)));
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
    /* Piecewise interpolation: these are artificial calibration points.
     * Reference 5-point board profile also starts with this linear map,
     * then replaces pulse_us values after actual angular measurements. */
    {
        ServoCalibrationPoint p[5]={
            {30000,1157u},{60000,1329u},{90000,1500u},
            {120000,1671u},{150000,1843u}
        };
        uint32_t pulse=0u;
        const uint32_t before=FTM1->CONTROLS[5].CnV;
        assert(Servo_SetCalibration(&a,p,5u)==SERVO_OK);
        assert(a.calibration_count==5u);
        /* Table activation alone never moves hardware. */
        assert(FTM1->CONTROLS[5].CnV==before);
        assert(Servo_SetAngle(&a,75000)==SERVO_OK);
        assert(Servo_GetPulseUs(&a,&pulse)==SERVO_OK && pulse==1415u);
        assert(FTM1->CONTROLS[5].CnV==3538u);
        assert(Servo_SetAngle(&a,45000)==SERVO_OK);
        assert(Servo_GetPulseUs(&a,&pulse)==SERVO_OK && pulse==1243u);
        assert(FTM1->CONTROLS[5].CnV==3108u);
        assert(Servo_SetAngle(&a,150000)==SERVO_OK);
        assert(Servo_GetPulseUs(&a,&pulse)==SERVO_OK && pulse==1843u);
        assert(Servo_SetAngle(&a,150001)==SERVO_ERR_RANGE);
        assert(Servo_SetAngle(&a,20000)==SERVO_ERR_RANGE);
        assert(Servo_GetPulseUs(&a,&pulse)==SERVO_OK && pulse==1843u);
        assert(Servo_SetCalibration(&a,p,1u)==SERVO_ERR_ARGUMENT);
        assert(Servo_SetCalibration(&a,p,6u)==SERVO_ERR_ARGUMENT);
        assert(Servo_SetCalibration(&a,NULL,5u)==SERVO_ERR_ARGUMENT);
        assert(a.calibration_count==5u);
        p[2].pulse_us=p[1].pulse_us;
        assert(Servo_SetCalibration(&a,p,5u)==SERVO_ERR_CALIBRATION);
        p[2].pulse_us=1500u;
        p[3].angle_mdeg=p[2].angle_mdeg;
        assert(Servo_SetCalibration(&a,p,5u)==SERVO_ERR_CALIBRATION);
        p[3].angle_mdeg=120000;
        p[4].pulse_us=2001u;
        assert(Servo_SetCalibration(&a,p,5u)==SERVO_ERR_RANGE);
        p[4].pulse_us=1843u;
        /* Current command=150 degrees: profile excluding it cannot activate. */
        assert(Servo_SetCalibration(&a,p,4u)==SERVO_ERR_RANGE);
        assert(a.calibration_count==5u);
        /* Direct pulse control bypasses calibrated angle mapping, used in
         * careful manual commissioning; get_pulse is the reliable command. */
        assert(Servo_SetPulseUs(&a,1600u)==SERVO_OK);
        assert(Servo_GetPulseUs(&a,&pulse)==SERVO_OK && pulse==1600u);
        assert(Servo_ClearCalibration(&a)==SERVO_OK);
        assert(a.calibration_count==0u);
        assert(Servo_SetAngle(&a,0)==SERVO_OK);
        assert(Servo_GetPulseUs(&a,&pulse)==SERVO_OK && pulse==1000u);
    }
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
    puts("PASS: FTM/channel, corrected CLKS, 5-point PWM interpolation, guards, limits");
    return 0;
}
