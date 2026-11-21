#ifndef ULTRASONIC_H
#define ULTRASONIC_H
#include <stdbool.h>
#include <stdint.h>
typedef enum { US_IDLE, US_TRIGGER_HIGH, US_WAIT_RISE, US_WAIT_FALL, US_DONE, US_TIMEOUT } UltrasonicState;
/* The board adapter owns GPIO and FTM capture hardware. All timestamps are
 * from one monotonically advancing 1-MHz timer, including wrap-around. */
typedef void (*UltrasonicTriggerWrite)(void *context, bool high);
typedef struct {
    UltrasonicTriggerWrite trigger_write;
    void *context;
    uint32_t trigger_us;
    uint32_t timeout_us;
    uint32_t min_echo_us;
    uint32_t max_echo_us;
} UltrasonicConfig;
typedef struct {
    UltrasonicConfig config;
    volatile UltrasonicState state;
    volatile uint32_t start_us;
    volatile uint32_t rise_us;
    volatile uint32_t echo_us;
} Ultrasonic;
bool Ultrasonic_Init(Ultrasonic *sensor, const UltrasonicConfig *config);
bool Ultrasonic_Start(Ultrasonic *sensor, uint32_t now_us);
void Ultrasonic_Update(Ultrasonic *sensor, uint32_t now_us);
void Ultrasonic_OnEchoEdge(Ultrasonic *sensor, bool high, uint32_t timestamp_us);
bool Ultrasonic_TakeResult(Ultrasonic *sensor, uint32_t *distance_mm, bool *valid);
#endif
