#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include "device_registers.h"
#include "timer_manager.h"
#include "stepper_controller.h"

MOCK_LPIT mock_lpit;
MOCK_PCC mock_pcc;
uint32_t mock_primask;
uint8_t mock_irq_enabled[52];
static PORT_Type mock_port;
static GPIO_Type mock_gpio;
void NVIC_EnableIRQ(IRQn_Type irq) {assert(irq>=48 && irq<=51);mock_irq_enabled[irq]=1u;}
void NVIC_DisableIRQ(IRQn_Type irq) {assert(irq>=48 && irq<=51);mock_irq_enabled[irq]=0u;}
void NVIC_ClearPendingIRQ(IRQn_Type irq) {assert(irq>=48 && irq<=51);}
uint32_t __get_PRIMASK(void) {return mock_primask;}
void __disable_irq(void) {mock_primask=1u;}
void __enable_irq(void) {mock_primask=0u;}
void MockTimer_Trigger(uint32_t ch);
static void dummy_delay(uint32_t ms) {assert(ms==0u);}
static void steps(StepperController *ctl,int n) {
    bool moving=false;
    for (int i=0;i<n;i++) MockTimer_Trigger((uint32_t)ctl->timer_channel);
    assert(StepperController_GetPosition(ctl,NULL,NULL,&moving)==STEPPER_OK);
}
int main(void) {
    const TimerManagerConfig tc={6u,40000000u,TIMER_CH3};
    StepMotorPin pins[4];
    StepMotor motor;
    StepperController ctl, rejected;
    StepperControllerConfig cfg;
    int32_t angle=0,position=0;
    bool moving=false;
    assert(TimerManager_Init(&tc)==TIMER_OK);
    for (uint8_t i=0u;i<4u;i++) {
        pins[i].port=&mock_port; pins[i].gpio=&mock_gpio;
        pins[i].number=(uint8_t)(i+4u); pins[i].pcc_index=PCC_PORTD_INDEX;
    }
    assert(StepMotor_Init(&motor,pins,dummy_delay)==0);
    cfg.motor=&motor; cfg.timer_channel=TIMER_CH1; cfg.period_us=2000u;
    cfg.steps_per_revolution=400u; cfg.minimum_angle_mdeg=-90000;
    cfg.maximum_angle_mdeg=90000;
    assert(StepperController_Init(&ctl,&cfg)==STEPPER_OK);
    assert(!StepperController_IsHomed(&ctl));
    assert(StepperController_SetTargetAngle(&ctl,90000)==STEPPER_ERR_NOT_READY);
    assert(StepperController_SetHome(&ctl,0)==STEPPER_OK);
    assert(StepperController_IsHomed(&ctl));
    assert(StepperController_SetTargetAngle(&ctl,90001)==STEPPER_ERR_RANGE);
    assert(StepperController_SetTargetAngle(&ctl,90000)==STEPPER_OK);
    steps(&ctl,30);
    assert(StepperController_GetPosition(&ctl,&angle,&position,&moving)==STEPPER_OK);
    assert(position==30 && angle==27000 && moving);
    assert(StepperController_SetHome(&ctl,0)==STEPPER_ERR_BUSY);
    assert(StepperController_SetTargetAngle(&ctl,-90000)==STEPPER_OK);
    steps(&ctl,130);
    assert(StepperController_GetPosition(&ctl,&angle,&position,&moving)==STEPPER_OK);
    assert(position==-100 && angle==-90000 && !moving);
    assert(StepperController_SetTargetAngle(&ctl,0)==STEPPER_OK);
    steps(&ctl,100);
    assert(StepperController_GetPosition(&ctl,&angle,&position,&moving)==STEPPER_OK);
    assert(position==0 && !moving);
    assert(StepperController_SetTargetAngle(&ctl,-90001)==STEPPER_ERR_RANGE);
    assert(StepperController_SetTargetAngle(&ctl,90000)==STEPPER_OK);
    steps(&ctl,5);
    assert(StepperController_Release(&ctl)==STEPPER_ERR_BUSY);
    assert(StepperController_Stop(&ctl)==STEPPER_OK);
    assert(StepperController_Release(&ctl)==STEPPER_OK);
    assert(StepperController_GetPosition(&ctl,&angle,&position,&moving)==STEPPER_OK);
    assert(position==5 && !moving);
    assert(StepperController_SetHome(&ctl,45000)==STEPPER_OK);
    assert(StepperController_GetPosition(&ctl,&angle,&position,&moving)==STEPPER_OK);
    assert(position==50 && angle==45000 && !moving);
    cfg.timer_channel=TIMER_CH3;
    assert(StepperController_Init(&rejected,&cfg)==STEPPER_ERR_TIMER);
    puts("PASS: channel, home, forward/reverse, retarget, stop, release, limits");
    return 0;
}
