#ifndef TIMER_MANAGER_H_
#define TIMER_MANAGER_H_

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    TIMER_CH_STEPPER = 0,
    TIMER_CH_TRIGGER = 1,
    TIMER_CH_SYSTEM = 2,
    TIMER_CH_RESERVED = 3
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

/* Call after SOSC_init_8MHz(), SPLL_init_160MHz(), NormalRUNmode_80MHz().
 * SPLLDIV2 = 40 MHz, LPIT0 exclusively owned by this module.
 * Init starts CH2 at 1 ms; CH0 and CH1 require Configure before Start.
 */
TimerResult TimerManager_Init(void);
TimerResult TimerManager_Configure(TimerChannel channel, uint32_t period_us, bool one_shot);
TimerResult TimerManager_RegisterCallback(TimerChannel channel, TimerCallback callback, void *context);
TimerResult TimerManager_Start(TimerChannel channel);
TimerResult TimerManager_Stop(TimerChannel channel);
uint32_t TimerManager_GetTickMs(void);
/* Display flags coalesce repeated requests; call these from foreground, not ISR. */
bool TimerManager_TakeOledRequest(void);
bool TimerManager_TakeLcdRequest(void);

#endif /* TIMER_MANAGER_H_ */
