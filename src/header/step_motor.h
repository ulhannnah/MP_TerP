#ifndef STEP_MOTOR_H_
#define STEP_MOTOR_H_

#include <stdint.h>
#include "device_registers.h"

#ifdef __cplusplus
extern "C" {
#endif

/* One driver input per GPIO pin. All four inputs may use different ports. */
typedef struct {
    PORT_Type *port;          /* e.g., PORTD */
    GPIO_Type *gpio;          /* e.g., PTD   */
    uint32_t pcc_index;       /* e.g., PCC_PORTD_INDEX */
    uint8_t number;           /* pin number (0..31) */
} StepMotorPin;

typedef void (*StepMotorDelayMs)(uint32_t ms);

typedef enum {
    STEP_MOTOR_HALF_STEP = 0, /* 8 states per electrical cycle */
    STEP_MOTOR_FULL_STEP = 1  /* 4 two-coil states per electrical cycle */
} StepMotorMode;

typedef enum {
    STEP_MOTOR_FORWARD = 0,
    STEP_MOTOR_REVERSE = 1
} StepMotorDirection;

typedef struct {
    StepMotorPin in[4];
    StepMotorDelayMs delay_ms;
    StepMotorMode mode;
    uint8_t phase;
    uint8_t initialized;
} StepMotor;

/* Returns 0 on success; -1 on invalid arguments. Initializes GPIO outputs LOW. */
int StepMotor_Init(StepMotor *motor, const StepMotorPin pins[4],
                   StepMotorDelayMs delay_ms);

/* Changing mode aligns the next full-step movement to a two-coil state. */
void StepMotor_SetMode(StepMotor *motor, StepMotorMode mode);

/* One half-step or full-step. delay_ms_per_step=0 skips the blocking delay. */
void StepMotor_Step(StepMotor *motor, StepMotorDirection direction,
                    uint32_t delay_ms_per_step);

/* Move a given number of half-steps or full-steps, as configured. Blocking. */
void StepMotor_Move(StepMotor *motor, StepMotorDirection direction,
                    uint32_t steps, uint32_t delay_ms_per_step);

/* Remove power from all four driver inputs; holding torque is lost. */
void StepMotor_Release(StepMotor *motor);

#ifdef __cplusplus
}
#endif

#endif /* STEP_MOTOR_H_ */
