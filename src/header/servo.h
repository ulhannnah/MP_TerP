#ifndef SERVO_H_
#define SERVO_H_
#include <stdbool.h>
#include <stdint.h>
#include "device_registers.h"

typedef enum {
    SERVO_OK = 0,
    SERVO_ERR_ARGUMENT = -1,
    SERVO_ERR_BUSY = -2,
    SERVO_ERR_CLOCK = -3,
    SERVO_ERR_RANGE = -4,
    SERVO_ERR_NOT_INITIALIZED = -5,
    SERVO_ERR_CALIBRATION = -6
} ServoResult;

/* Five command-angle to PWM pulse pairs, copied into Servo (no borrowed pointer).
 * Angles are commanded positions, NOT measured feedback. */
#define SERVO_MAX_CALIBRATION_POINTS 5u
typedef struct {
    int32_t angle_mdeg;
    uint32_t pulse_us;
} ServoCalibrationPoint;

/* Exclusive ownership of the entire FTM instance (period, clock and counter).
 * Output pin ALT must be checked against the actual MCU package and connector.
 */
typedef struct {
    FTM_Type *ftm;              /* FTM0..FTM3 */
    uint8_t channel;            /* FTM channel */
    PORT_Type *port;            /* GPIO PORT peripheral */
    uint32_t port_pcc_index;    /* PCC_PORTx_INDEX */
    uint8_t pin;                /* PORT pin number */
    uint8_t pin_alt;            /* corresponding FTM ALT value */
    uint8_t clock_pcs;          /* FTM PCC PCS */
    uint32_t clock_hz;          /* actual selected FTM functional clock */
    uint32_t period_us;         /* typically 20000 us (50 Hz) */
    uint32_t minimum_pulse_us;
    uint32_t maximum_pulse_us;
    int32_t minimum_angle_mdeg;
    int32_t maximum_angle_mdeg;
    int32_t initial_angle_mdeg;
} ServoConfig;

typedef struct {
    FTM_Type *ftm;
    uint8_t channel;
    uint8_t ftm_index;
    uint8_t prescaler;
    uint8_t initialized;
    uint8_t running;
    uint32_t period_ticks;
    uint32_t clock_hz;
    uint32_t minimum_pulse_us;
    uint32_t maximum_pulse_us;
    int32_t minimum_angle_mdeg;
    int32_t maximum_angle_mdeg;
    int32_t angle_mdeg; /* most recent requested/estimated angle, not measured */
    uint32_t last_pulse_us;
    uint8_t calibration_count; /* 0 = original linear map */
    ServoCalibrationPoint calibration[SERVO_MAX_CALIBRATION_POINTS];
} Servo;

/* Call after clocks_and_modes. No servo settling delay is performed. */
ServoResult Servo_Init(Servo *servo, const ServoConfig *config);
/* Enable 2..5 strictly increasing angle/pulse pairs. It does not move the
 * servo immediately; next Servo_SetAngle() uses piecewise interpolation.
 * With calibration active, angles outside the first/last pair are rejected
 * rather than extrapolated. Pass no invalid measurements or endpoints. */
ServoResult Servo_SetCalibration(Servo *servo,
                                 const ServoCalibrationPoint *points,
                                 uint8_t count);
/* Disable calibration, returning to the original min/max linear mapping. */
ServoResult Servo_ClearCalibration(Servo *servo);
ServoResult Servo_SetAngle(Servo *servo, int32_t angle_mdeg);
ServoResult Servo_SetPulseUs(Servo *servo, uint32_t pulse_us);
ServoResult Servo_Start(Servo *servo);
ServoResult Servo_Stop(Servo *servo);
ServoResult Servo_Deinit(Servo *servo);
ServoResult Servo_GetAngle(const Servo *servo, int32_t *angle_mdeg);
/* Last PWM command (not a physical output measurement). */
ServoResult Servo_GetPulseUs(const Servo *servo, uint32_t *pulse_us);
#endif
