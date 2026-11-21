#include "servo.h"
#include <stddef.h>

#define SERVO_MAX_TICKS 65536u
#define SERVO_PWM_SOURCE 1u /* FTM functional clock */

/* Each instance is dedicated to one Servo object in this version.
 * FTM channels share MOD, counter and prescaler.
 */
static uint8_t s_ftm_claimed[4];

static int ftm_index(const FTM_Type *ftm)
{
    if (ftm == FTM0) return 0;
    if (ftm == FTM1) return 1;
    if (ftm == FTM2) return 2;
    if (ftm == FTM3) return 3;
    return -1;
}

static uint32_t ftm_pcc_index(uint32_t index)
{
    switch (index) {
        case 0u: return PCC_FTM0_INDEX;
        case 1u: return PCC_FTM1_INDEX;
        case 2u: return PCC_FTM2_INDEX;
        default: return PCC_FTM3_INDEX;
    }
}

/* Rounded tick conversion with no floating-point in hardware control path. */
static uint32_t pulse_ticks(const Servo *servo, uint32_t pulse_us)
{
    const uint64_t divisor = ((uint64_t)1000000u << servo->prescaler);
    return (uint32_t)(((uint64_t)servo->clock_hz * pulse_us + divisor / 2u) / divisor);
}

static uint32_t angle_pulse_us(const Servo *servo, int32_t angle)
{
    const uint64_t angle_range =
        (uint32_t)(servo->maximum_angle_mdeg - servo->minimum_angle_mdeg);
    const uint64_t offset = (uint32_t)(angle - servo->minimum_angle_mdeg);
    const uint64_t pulse_span = servo->maximum_pulse_us - servo->minimum_pulse_us;
    return servo->minimum_pulse_us +
        (uint32_t)((pulse_span * offset + angle_range / 2u) / angle_range);
}

static uint32_t pulse_angle_offset(const Servo *servo, uint32_t pulse_us)
{
    const uint64_t pulse_range = servo->maximum_pulse_us - servo->minimum_pulse_us;
    const uint64_t angle_range =
        (uint32_t)(servo->maximum_angle_mdeg - servo->minimum_angle_mdeg);
    const uint64_t offset = pulse_us - servo->minimum_pulse_us;
    return (uint32_t)((angle_range * offset + pulse_range / 2u) / pulse_range);
}

ServoResult Servo_Init(Servo *servo, const ServoConfig *cfg)
{
    int index;
    uint32_t period_ticks = 0u;
    uint8_t ps;
    uint32_t pcc_index;
    uint32_t init_pulse;

    if (servo == NULL || cfg == NULL) return SERVO_ERR_ARGUMENT;
    index = ftm_index(cfg->ftm);
    if (index < 0 || cfg->channel > 7u || cfg->port == NULL ||
        cfg->pin >= 32u || cfg->pin_alt < 2u || cfg->pin_alt > 7u ||
        cfg->clock_pcs == 0u || cfg->clock_pcs > 7u || cfg->clock_hz == 0u ||
        cfg->period_us == 0u || cfg->minimum_pulse_us == 0u ||
        cfg->minimum_pulse_us >= cfg->maximum_pulse_us ||
        cfg->maximum_pulse_us >= cfg->period_us ||
        cfg->minimum_angle_mdeg >= cfg->maximum_angle_mdeg ||
        cfg->minimum_angle_mdeg < -360000 ||
        cfg->maximum_angle_mdeg > 360000 ||
        cfg->initial_angle_mdeg < cfg->minimum_angle_mdeg ||
        cfg->initial_angle_mdeg > cfg->maximum_angle_mdeg ||
        cfg->port_pcc_index >= 128u) return SERVO_ERR_ARGUMENT;
    if (s_ftm_claimed[index] != 0u) return SERVO_ERR_BUSY;

    /* Select smallest prescaler (1..128) that fits 16-bit MOD. */
    for (ps = 0u; ps < 8u; ++ps) {
        const uint64_t divisor = (uint64_t)1000000u << ps;
        const uint64_t ticks =
            ((uint64_t)cfg->clock_hz * cfg->period_us + divisor / 2u) / divisor;
        if (ticks >= 2u && ticks <= SERVO_MAX_TICKS) {
            period_ticks = (uint32_t)ticks;
            break;
        }
    }
    if (period_ticks == 0u) return SERVO_ERR_CLOCK;

    servo->ftm = cfg->ftm;
    servo->channel = cfg->channel;
    servo->ftm_index = (uint8_t)index;
    servo->prescaler = ps;
    servo->period_ticks = period_ticks;
    servo->clock_hz = cfg->clock_hz;
    servo->minimum_pulse_us = cfg->minimum_pulse_us;
    servo->maximum_pulse_us = cfg->maximum_pulse_us;
    servo->minimum_angle_mdeg = cfg->minimum_angle_mdeg;
    servo->maximum_angle_mdeg = cfg->maximum_angle_mdeg;
    servo->angle_mdeg = cfg->initial_angle_mdeg;
    servo->running = 0u;
    servo->initialized = 0u;
    init_pulse = angle_pulse_us(servo, cfg->initial_angle_mdeg);
    if (pulse_ticks(servo, cfg->maximum_pulse_us) >= period_ticks ||
        pulse_ticks(servo, cfg->minimum_pulse_us) == 0u) return SERVO_ERR_CLOCK;

    s_ftm_claimed[index] = 1u;
    PCC->PCCn[cfg->port_pcc_index] |= PCC_PCCn_CGC_MASK;
    pcc_index = ftm_pcc_index((uint32_t)index);
    PCC->PCCn[pcc_index] &= ~PCC_PCCn_CGC_MASK;
    PCC->PCCn[pcc_index] =
        (PCC->PCCn[pcc_index] & ~PCC_PCCn_PCS_MASK) |
        PCC_PCCn_PCS(cfg->clock_pcs) | PCC_PCCn_CGC_MASK;

    cfg->ftm->SC = 0u;
    cfg->ftm->MODE |= FTM_MODE_WPDIS_MASK;
    cfg->ftm->CNTIN = 0u;
    cfg->ftm->MOD = period_ticks - 1u;
    cfg->ftm->CONTROLS[cfg->channel].CnSC =
        FTM_CnSC_MSB_MASK | FTM_CnSC_ELSB_MASK;
    cfg->ftm->CONTROLS[cfg->channel].CnV = pulse_ticks(servo, init_pulse);
    cfg->ftm->CNT = 0u;
    cfg->port->PCR[cfg->pin] = PORT_PCR_MUX(cfg->pin_alt);
    servo->initialized = 1u;
    return Servo_Start(servo);
}

ServoResult Servo_SetAngle(Servo *servo, int32_t angle_mdeg)
{
    uint32_t pulse;
    if (servo == NULL || !servo->initialized) return SERVO_ERR_NOT_INITIALIZED;
    if (angle_mdeg < servo->minimum_angle_mdeg ||
        angle_mdeg > servo->maximum_angle_mdeg) return SERVO_ERR_RANGE;
    pulse = angle_pulse_us(servo, angle_mdeg);
    servo->ftm->CONTROLS[servo->channel].CnV = pulse_ticks(servo, pulse);
    servo->angle_mdeg = angle_mdeg;
    return SERVO_OK;
}

ServoResult Servo_SetPulseUs(Servo *servo, uint32_t pulse_us)
{
    if (servo == NULL || !servo->initialized) return SERVO_ERR_NOT_INITIALIZED;
    if (pulse_us < servo->minimum_pulse_us ||
        pulse_us > servo->maximum_pulse_us) return SERVO_ERR_RANGE;
    servo->ftm->CONTROLS[servo->channel].CnV = pulse_ticks(servo, pulse_us);
    servo->angle_mdeg = servo->minimum_angle_mdeg +
        (int32_t)pulse_angle_offset(servo, pulse_us);
    return SERVO_OK;
}

ServoResult Servo_Start(Servo *servo)
{
    if (servo == NULL || !servo->initialized) return SERVO_ERR_NOT_INITIALIZED;
    servo->ftm->SC =
        FTM_SC_PS(servo->prescaler) |
        FTM_SC_CLKS(SERVO_PWM_SOURCE) |
        ((uint32_t)FTM_SC_PWMEN0_MASK << servo->channel);
    servo->running = 1u;
    return SERVO_OK;
}

ServoResult Servo_Stop(Servo *servo)
{
    if (servo == NULL || !servo->initialized) return SERVO_ERR_NOT_INITIALIZED;
    servo->ftm->SC = 0u;
    servo->running = 0u;
    /* Stopping the counter does not guarantee a low physical output. */
    return SERVO_OK;
}

ServoResult Servo_Deinit(Servo *servo)
{
    if (servo == NULL || !servo->initialized) return SERVO_ERR_NOT_INITIALIZED;
    (void)Servo_Stop(servo);
    s_ftm_claimed[servo->ftm_index] = 0u;
    servo->initialized = 0u;
    return SERVO_OK;
}

ServoResult Servo_GetAngle(const Servo *servo, int32_t *angle_mdeg)
{
    if (servo == NULL || !servo->initialized) return SERVO_ERR_NOT_INITIALIZED;
    if (angle_mdeg == NULL) return SERVO_ERR_ARGUMENT;
    *angle_mdeg = servo->angle_mdeg;
    return SERVO_OK;
}
