#include "step_motor.h"
#include <stddef.h>
int StepMotor_Init(StepMotor *motor,const StepMotorPin pins[4],StepMotorDelayMs delay) {
    if (!motor || !pins || !delay) return -1;
    for (int i=0;i<4;i++) motor->in[i]=pins[i];
    motor->initialized=1u;motor->mode=STEP_MOTOR_HALF_STEP;motor->delay_ms=delay;motor->phase=7u;return 0;
}
void StepMotor_SetMode(StepMotor *motor,StepMotorMode mode) {motor->mode=mode;}
void StepMotor_Step(StepMotor *motor,StepMotorDirection d,uint32_t ms) {
    if (!motor || !motor->initialized) return;
    uint8_t advance=motor->mode==STEP_MOTOR_FULL_STEP?2u:1u;
    motor->phase=(uint8_t)((motor->phase + (d==STEP_MOTOR_FORWARD?advance:8u-advance))&7u);
    if (ms) motor->delay_ms(ms);
}
void StepMotor_Move(StepMotor *motor,StepMotorDirection d,uint32_t n,uint32_t ms) {
    for (uint32_t i=0;i<n;i++) StepMotor_Step(motor,d,ms);
}
void StepMotor_Release(StepMotor *motor) {(void)motor;}
