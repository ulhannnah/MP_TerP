#ifndef STEPPER_CONTROLLER_H_
#define STEPPER_CONTROLLER_H_

#include <stdbool.h>
#include <stdint.h>
#include "step_motor.h"
#include "timer_manager.h"

typedef enum {
    STEPPER_OK = 0,
    STEPPER_ERR_ARGUMENT = -1,
    STEPPER_ERR_NOT_READY = -2,
    STEPPER_ERR_RANGE = -3,
    STEPPER_ERR_TIMER = -4,
    STEPPER_ERR_BUSY = -5
} StepperResult;

/* Angles in millidegrees relative to a user-defined mechanical reference.
 * steps_per_revolution: output-shaft steps / 360 degrees for the selected
 * motor, gearing, and half/full-step mode. Position is OPEN-LOOP (no encoder).
 */
typedef struct {
    StepMotor *motor;                    /* Already StepMotor_Init()'d */
    TimerChannel timer_channel;          /* Any free, non-system-tick LPIT channel */
    uint32_t period_us;                  /* Step interval */
    uint32_t steps_per_revolution;       /* Calibrate for physical hardware */
    int32_t minimum_angle_mdeg;
    int32_t maximum_angle_mdeg;
} StepperControllerConfig;

typedef struct {
    StepMotor *motor;
    TimerChannel timer_channel;
    uint32_t steps_per_revolution;
    int32_t minimum_angle_mdeg;
    int32_t maximum_angle_mdeg;
    volatile int32_t current_steps;
    volatile int32_t target_steps;
    volatile uint8_t moving;
    volatile uint8_t homed;
    uint8_t initialized;
} StepperController;

/* TimerManager_Init() first. One controller per exclusively owned LPIT channel.
 * Motor must be initialized with a valid delay callback in the existing driver.
 * All methods except the internal step callback are foreground APIs.
 */
StepperResult StepperController_Init(StepperController *ctl,
                                    const StepperControllerConfig *config);

/* Declare a known mechanical reference while stopped; no physical homing. */
StepperResult StepperController_SetHome(StepperController *ctl, int32_t known_angle_mdeg);
/* Non-blocking target update, including while moving. */
StepperResult StepperController_SetTargetAngle(StepperController *ctl, int32_t angle_mdeg);
StepperResult StepperController_Stop(StepperController *ctl);
StepperResult StepperController_Release(StepperController *ctl);
StepperResult StepperController_GetPosition(StepperController *ctl,
                                           int32_t *angle_mdeg,
                                           int32_t *steps,
                                           bool *moving);
bool StepperController_IsHomed(const StepperController *ctl);

#endif /* STEPPER_CONTROLLER_H_ */
