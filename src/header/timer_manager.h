#ifndef TIMER_MANAGER_H_
#define TIMER_MANAGER_H_

#include <stdbool.h>
#include <stdint.h>

/* Hardware channels, not application functions. No channel is hard-wired to a device. */
typedef enum {
    TIMER_CH0 = 0,
    TIMER_CH1 = 1,
    TIMER_CH2 = 2,
    TIMER_CH3 = 3,
    TIMER_CHANNEL_NONE = 255
} TimerChannel;

typedef enum {
    TIMER_OK = 0,
    TIMER_ERR_ARGUMENT = -1,
    TIMER_ERR_RANGE = -2,
    TIMER_ERR_RESERVED = -3,
    TIMER_ERR_NOT_INITIALIZED = -4,
    TIMER_ERR_NOT_CONFIGURED = -5,
    TIMER_ERR_BUSY = -6
} TimerResult;

typedef void (*TimerCallback)(void *context);

/* Initialize LPIT0. Clock source is PCC PCS (0..7); hz must reflect the
 * selected clock source and be an integer multiple of 1 MHz.
 * Example: { 6u, 40000000u, TIMER_CH2 } for SPLLDIV2 40 MHz.
 * Set tick_channel=TIMER_CHANNEL_NONE to reserve no system tick channel.
 * One selected channel (if any) generates a 1 ms monotonic counter.
 * Must be called after clocks_and_modes initialization.
 */
typedef struct {
    uint8_t pcc_clock_source;
    uint32_t clock_hz;
    TimerChannel tick_channel;
} TimerManagerConfig;

TimerResult TimerManager_Init(const TimerManagerConfig *config);

/* All non-tick channels (CH0..CH3) are independently configurable.
 * Configure leaves the timer STOPPED, and stops any previous operation.
 * one_shot=true stops the channel at timeout (LPIT TSOI).
 * Callbacks execute directly in IRQ context: keep them very short.
 */
TimerResult TimerManager_Configure(TimerChannel channel, uint32_t period_us, bool one_shot);
TimerResult TimerManager_RegisterCallback(TimerChannel channel, TimerCallback callback, void *context);
TimerResult TimerManager_Start(TimerChannel channel);
TimerResult TimerManager_Stop(TimerChannel channel);
uint32_t TimerManager_GetTickMs(void);

#endif /* TIMER_MANAGER_H_ */
