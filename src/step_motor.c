#include "step_motor.h"
#include <stddef.h>

/* Active-HIGH, 4-wire control inputs: IN1 = bit 0 ... IN4 = bit 3.
 * Suitable for the already-tested half-step connection. Actual driver/wiring
 * may require a different sequence or signal polarity.
 */
static const uint8_t s_half_step[8] = {
    0x01u, 0x03u, 0x02u, 0x06u,
    0x04u, 0x0Cu, 0x08u, 0x09u
};

static void StepMotor_Output(const StepMotor *motor, uint8_t pattern)
{
    uint8_t i;

    /* Turn off coils that must become LOW before enabling new HIGH inputs. */
    for (i = 0u; i < 4u; ++i) {
        if ((pattern & (uint8_t)(1u << i)) == 0u) {
            motor->in[i].gpio->PCOR = (uint32_t)1u << motor->in[i].number;
        }
    }

    for (i = 0u; i < 4u; ++i) {
        if ((pattern & (uint8_t)(1u << i)) != 0u) {
            motor->in[i].gpio->PSOR = (uint32_t)1u << motor->in[i].number;
        }
    }
}

int StepMotor_Init(StepMotor *motor, const StepMotorPin pins[4],
                   StepMotorDelayMs delay_ms)
{
    uint8_t i;

    if ((motor == NULL) || (pins == NULL) || (delay_ms == NULL)) {
        return -1;
    }

    for (i = 0u; i < 4u; ++i) {
        if ((pins[i].port == NULL) || (pins[i].gpio == NULL) ||
            (pins[i].number >= 32u)) {
            return -1;
        }
    }

    motor->initialized = 0u;
    motor->delay_ms = delay_ms;
    motor->mode = STEP_MOTOR_HALF_STEP;
    motor->phase = 7u;  /* First forward step outputs half-step state 0. */

    for (i = 0u; i < 4u; ++i) {
        uint32_t mask;
        motor->in[i] = pins[i];
        mask = (uint32_t)1u << motor->in[i].number;

        /* PCC and PORT/PDDR are different registers: avoid touching other pins. */
        PCC->PCCn[motor->in[i].pcc_index] |= PCC_PCCn_CGC_MASK;
        motor->in[i].gpio->PCOR = mask;          /* Start LOW */
        motor->in[i].port->PCR[motor->in[i].number] = PORT_PCR_MUX(1);
        motor->in[i].gpio->PDDR |= mask;         /* GPIO output */
    }

    motor->initialized = 1u;
    return 0;
}

void StepMotor_SetMode(StepMotor *motor, StepMotorMode mode)
{
    if ((motor == NULL) || (motor->initialized == 0u)) {
        return;
    }
    if ((mode != STEP_MOTOR_HALF_STEP) &&
        (mode != STEP_MOTOR_FULL_STEP)) {
        return;
    }

    motor->mode = mode;
    if (mode == STEP_MOTOR_FULL_STEP) {
        motor->phase |= 1u;  /* odd phases have both coils active */
    }
}

void StepMotor_Step(StepMotor *motor, StepMotorDirection direction,
                    uint32_t delay_ms_per_step)
{
    uint8_t advance;

    if ((motor == NULL) || (motor->initialized == 0u)) {
        return;
    }
    if ((direction != STEP_MOTOR_FORWARD) &&
        (direction != STEP_MOTOR_REVERSE)) {
        return;
    }

    advance = (motor->mode == STEP_MOTOR_FULL_STEP) ? 2u : 1u;

    if (direction == STEP_MOTOR_FORWARD) {
        motor->phase = (uint8_t)((motor->phase + advance) & 7u);
    } else {
        motor->phase = (uint8_t)((motor->phase + 8u - advance) & 7u);
    }

    StepMotor_Output(motor, s_half_step[motor->phase]);

    if (delay_ms_per_step != 0u) {
        motor->delay_ms(delay_ms_per_step);
    }
}

void StepMotor_Move(StepMotor *motor, StepMotorDirection direction,
                    uint32_t steps, uint32_t delay_ms_per_step)
{
    uint32_t i;
    for (i = 0u; i < steps; ++i) {
        StepMotor_Step(motor, direction, delay_ms_per_step);
    }
}

void StepMotor_Release(StepMotor *motor)
{
    uint8_t i;
    if ((motor == NULL) || (motor->initialized == 0u)) {
        return;
    }
    for (i = 0u; i < 4u; ++i) {
        motor->in[i].gpio->PCOR = (uint32_t)1u << motor->in[i].number;
    }
}
