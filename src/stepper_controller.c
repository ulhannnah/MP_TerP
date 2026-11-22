#include "stepper_controller.h"
#include "mp_irq_compat.h"
#include <stddef.h>

#define MILLIDEGREES_PER_REVOLUTION 360000LL

static uint32_t lock_irqs(void)
{
    return MP_IrqSave();
}

static void unlock_irqs(uint32_t mask)
{
    MP_IrqRestore(mask);
}

/* Conversion occurs in foreground only; the timer ISR uses integer steps. */
static int32_t angle_to_steps(int32_t angle, uint32_t steps_per_rev)
{
    int64_t product = (int64_t)angle * (int64_t)steps_per_rev;
    if (product >= 0) product += MILLIDEGREES_PER_REVOLUTION / 2;
    else product -= MILLIDEGREES_PER_REVOLUTION / 2;
    return (int32_t)(product / MILLIDEGREES_PER_REVOLUTION);
}

static int32_t steps_to_angle(int32_t steps, uint32_t steps_per_rev)
{
    int64_t product = (int64_t)steps * MILLIDEGREES_PER_REVOLUTION;
    if (product >= 0) product += (int64_t)steps_per_rev / 2;
    else product -= (int64_t)steps_per_rev / 2;
    return (int32_t)(product / (int64_t)steps_per_rev);
}

/* LPIT IRQ context: exactly one physical step, no delay or floating point. */
static void on_step_timer(void *context)
{
    StepperController *ctl = (StepperController *)context;
    int32_t current, target;
    if (ctl == NULL || !ctl->initialized || !ctl->moving) return;
    current = ctl->current_steps;
    target = ctl->target_steps;
    if (current == target) {
        ctl->moving = 0u;
        (void)TimerManager_Stop(ctl->timer_channel);
        return;
    }
    if (current < target) {
        StepMotor_Step(ctl->motor, STEP_MOTOR_FORWARD, 0u);
        ctl->current_steps = current + 1;
    } else {
        StepMotor_Step(ctl->motor, STEP_MOTOR_REVERSE, 0u);
        ctl->current_steps = current - 1;
    }
    if (ctl->current_steps == ctl->target_steps) {
        ctl->moving = 0u;
        (void)TimerManager_Stop(ctl->timer_channel);
    }
}

StepperResult StepperController_Init(StepperController *ctl,
                                    const StepperControllerConfig *config)
{
    TimerResult tr;
    if (ctl == NULL || config == NULL || config->motor == NULL ||
        config->motor->initialized == 0u ||
        config->period_us == 0u || config->steps_per_revolution == 0u ||
        config->steps_per_revolution > 1000000u ||
        config->minimum_angle_mdeg >= config->maximum_angle_mdeg ||
        config->minimum_angle_mdeg < -360000 ||
        config->maximum_angle_mdeg > 360000 ||
        (unsigned)config->timer_channel > 3u) return STEPPER_ERR_ARGUMENT;

    ctl->initialized = 0u;
    ctl->motor = config->motor;
    ctl->timer_channel = config->timer_channel;
    ctl->steps_per_revolution = config->steps_per_revolution;
    ctl->minimum_angle_mdeg = config->minimum_angle_mdeg;
    ctl->maximum_angle_mdeg = config->maximum_angle_mdeg;
    ctl->current_steps = 0;
    ctl->target_steps = 0;
    ctl->homed = 0u;
    ctl->moving = 0u;
    tr = TimerManager_Configure(ctl->timer_channel, config->period_us, false);
    if (tr != TIMER_OK) return STEPPER_ERR_TIMER;
    tr = TimerManager_RegisterCallback(ctl->timer_channel, on_step_timer, ctl);
    if (tr != TIMER_OK) return STEPPER_ERR_TIMER;
    ctl->initialized = 1u;
    return STEPPER_OK;
}

StepperResult StepperController_SetHome(StepperController *ctl, int32_t known_angle_mdeg)
{
    uint32_t irqmask;
    if (ctl == NULL || !ctl->initialized) return STEPPER_ERR_NOT_READY;
    if (known_angle_mdeg < ctl->minimum_angle_mdeg ||
        known_angle_mdeg > ctl->maximum_angle_mdeg) return STEPPER_ERR_RANGE;
    irqmask = lock_irqs();
    if (ctl->moving) {
        unlock_irqs(irqmask);
        return STEPPER_ERR_BUSY;
    }
    ctl->current_steps = angle_to_steps(known_angle_mdeg, ctl->steps_per_revolution);
    ctl->target_steps = ctl->current_steps;
    ctl->homed = 1u;
    unlock_irqs(irqmask);
    return STEPPER_OK;
}

StepperResult StepperController_SetTargetAngle(StepperController *ctl, int32_t angle_mdeg)
{
    uint32_t irqmask;
    uint8_t need_start = 0u;
    TimerResult tr;
    int32_t steps;
    if (ctl == NULL || !ctl->initialized || !ctl->homed) return STEPPER_ERR_NOT_READY;
    if (angle_mdeg < ctl->minimum_angle_mdeg ||
        angle_mdeg > ctl->maximum_angle_mdeg) return STEPPER_ERR_RANGE;
    steps = angle_to_steps(angle_mdeg, ctl->steps_per_revolution);
    irqmask = lock_irqs();
    ctl->target_steps = steps;
    if (!ctl->moving && ctl->current_steps != steps) {
        ctl->moving = 1u;
        need_start = 1u;
    }
    unlock_irqs(irqmask);
    if (need_start) {
        tr = TimerManager_Start(ctl->timer_channel);
        if (tr != TIMER_OK) {
            irqmask = lock_irqs();
            ctl->moving = 0u;
            unlock_irqs(irqmask);
            return STEPPER_ERR_TIMER;
        }
    }
    return STEPPER_OK;
}

StepperResult StepperController_Stop(StepperController *ctl)
{
    uint32_t irqmask;
    if (ctl == NULL || !ctl->initialized) return STEPPER_ERR_NOT_READY;
    irqmask = lock_irqs();
    (void)TimerManager_Stop(ctl->timer_channel);
    ctl->moving = 0u;
    ctl->target_steps = ctl->current_steps;
    unlock_irqs(irqmask);
    return STEPPER_OK;
}

StepperResult StepperController_Release(StepperController *ctl)
{
    uint32_t irqmask;
    if (ctl == NULL || !ctl->initialized) return STEPPER_ERR_NOT_READY;
    irqmask = lock_irqs();
    if (ctl->moving) {
        unlock_irqs(irqmask);
        return STEPPER_ERR_BUSY;
    }
    StepMotor_Release(ctl->motor);
    unlock_irqs(irqmask);
    return STEPPER_OK;
}

StepperResult StepperController_GetPosition(StepperController *ctl,
                                           int32_t *angle_mdeg,
                                           int32_t *steps,
                                           bool *moving)
{
    uint32_t irqmask;
    int32_t snap_steps;
    if (ctl == NULL || !ctl->initialized || !ctl->homed) return STEPPER_ERR_NOT_READY;
    if (angle_mdeg == NULL && steps == NULL && moving == NULL) return STEPPER_ERR_ARGUMENT;
    irqmask = lock_irqs();
    snap_steps = ctl->current_steps;
    if (steps != NULL) *steps = snap_steps;
    if (moving != NULL) *moving = (ctl->moving != 0u);
    unlock_irqs(irqmask);
    if (angle_mdeg != NULL)
        *angle_mdeg = steps_to_angle(snap_steps, ctl->steps_per_revolution);
    return STEPPER_OK;
}

bool StepperController_IsHomed(const StepperController *ctl)
{
    return (ctl != NULL && ctl->initialized != 0u && ctl->homed != 0u);
}
