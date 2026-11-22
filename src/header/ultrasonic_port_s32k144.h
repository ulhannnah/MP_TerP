#ifndef ULTRASONIC_PORT_S32K144_H
#define ULTRASONIC_PORT_S32K144_H
#include <stdbool.h>
#include <stdint.h>
#include "device_registers.h"
#include "timer_manager.h"
#include "ultrasonic.h"

/* Dedicated FTM instance is required for this adapter. It cannot share the
 * instance used by Servo. An FTM pair IRQ handler in the board project must
 * call UltrasonicPort_OnCaptureIrq(); this file does not claim a fixed vector.
 */
typedef struct {
    FTM_Type *ftm;
    uint8_t ftm_channel;
    uint8_t ftm_clock_pcs;
    uint32_t ftm_clock_hz;
    PORT_Type *echo_port;
    uint32_t echo_port_pcc_index;
    uint8_t echo_pin;
    uint8_t echo_pin_alt;
    PORT_Type *trigger_port;
    GPIO_Type *trigger_gpio;
    uint32_t trigger_port_pcc_index;
    uint8_t trigger_pin;
    TimerChannel trigger_timer_channel; /* Different from system/stepper LPIT */
    uint32_t trigger_us;
    uint32_t timeout_us;
    uint32_t minimum_echo_us;
    uint32_t maximum_echo_us;
} UltrasonicPortConfig;
typedef struct {
    Ultrasonic sensor;
    UltrasonicPortConfig cfg;
    uint32_t ftm_frequency_hz;
    uint8_t prescaler;
    uint8_t initialized;
} UltrasonicPort;

bool UltrasonicPort_Init(UltrasonicPort *port,const UltrasonicPortConfig *cfg);
/* now_ms must come from a monotonically incrementing 1 ms system timer. */
bool UltrasonicPort_Start(UltrasonicPort *port,uint32_t now_ms);
void UltrasonicPort_Update(UltrasonicPort *port,uint32_t now_ms);
bool UltrasonicPort_Take(UltrasonicPort *port,uint32_t *distance_mm,bool *valid);
/* Call once from the selected FTM channel-pair NVIC handler. */
void UltrasonicPort_OnCaptureIrq(UltrasonicPort *port);
#endif
