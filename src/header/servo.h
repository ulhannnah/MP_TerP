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
    SERVO_ERR_NOT_INITIALIZED = -5
} ServoResult;

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
    int32_t angle_mdeg;
} Servo;

/* Call after clocks_and_modes. No servo settling delay is performed. */
ServoResult Servo_Init(Servo *servo, const ServoConfig *config);
ServoResult Servo_SetAngle(Servo *servo, int32_t angle_mdeg);
ServoResult Servo_SetPulseUs(Servo *servo, uint32_t pulse_us);
ServoResult Servo_Start(Servo *servo);
ServoResult Servo_Stop(Servo *servo);
ServoResult Servo_Deinit(Servo *servo);
ServoResult Servo_GetAngle(const Servo *servo, int32_t *angle_mdeg);
#endif
