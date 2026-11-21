#ifndef TIMER_MANAGER_H_
#define TIMER_MANAGER_H_

#include <stdbool.h>
#include <stdint.h>

/* LPIT0 allocation: CH0 stepper, CH1 one-shot, CH2 system tick, CH3 reserved. */
typedef enum {
    TIMER_CH_STEPPER = 0,
    TIMER_CH_TRIGGER = 1,
    TIMER_CH_SYSTEM  = 2,
    TIMER_CH_RESERVED = 3
} TimerChannel;

typedef enum {
    TIMER_OK = 0,
    TIMER_ERR_ARGUMENT = -1,
    TIMER_ERR_RANGE = -2,
    TIMER_ERR_RESERVED = -3,
    TIMER_ERR_NOT_INITIALIZED = -4
} TimerResult;

typedef void (*TimerCallback)(void *context);

TimerResult TimerManager_Init(void);
TimerResult TimerManager_Configure(TimerChannel channel, uint32_t period_us, bool one_shot);
TimerResult TimerManager_RegisterCallback(TimerChannel channel, TimerCallback callback, void *context);
TimerResult TimerManager_Start(TimerChannel channel);
TimerResult TimerManager_Stop(TimerChannel channel);
uint32_t TimerManager_GetTickMs(void);
bool TimerManager_TakeOledRequest(void);
bool TimerManager_TakeLcdRequest(void);

#endif /* TIMER_MANAGER_H_ */
