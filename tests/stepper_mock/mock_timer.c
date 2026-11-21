#include "timer_manager.h"
#include <stdint.h>
#include <stddef.h>
static TimerCallback s_cb[4];
static void *s_ctx[4];
static uint8_t s_configured[4], s_running[4];
static TimerChannel s_reserved = TIMER_CHANNEL_NONE;
TimerResult TimerManager_Init(const TimerManagerConfig *cfg) {
    if (!cfg) return TIMER_ERR_ARGUMENT;
    s_reserved=cfg->tick_channel;
    for (int i=0;i<4;i++) {s_cb[i]=NULL;s_ctx[i]=NULL;s_configured[i]=0;s_running[i]=0;}
    return TIMER_OK;
}
TimerResult TimerManager_Configure(TimerChannel ch,uint32_t us,bool one_shot) {
    (void)one_shot;
    if ((unsigned)ch>=4u) return TIMER_ERR_ARGUMENT;
    if (ch==s_reserved) return TIMER_ERR_RESERVED;
    if (us==0u) return TIMER_ERR_RANGE;
    s_configured[ch]=1u;s_running[ch]=0u;return TIMER_OK;
}
TimerResult TimerManager_RegisterCallback(TimerChannel ch,TimerCallback cb,void *ctx) {
    if ((unsigned)ch>=4u) return TIMER_ERR_ARGUMENT;
    if (ch==s_reserved) return TIMER_ERR_RESERVED;
    if (s_running[ch]) return TIMER_ERR_BUSY;
    s_cb[ch]=cb;s_ctx[ch]=ctx;return TIMER_OK;
}
TimerResult TimerManager_Start(TimerChannel ch) {
    if ((unsigned)ch>=4u) return TIMER_ERR_ARGUMENT;
    if (ch==s_reserved) return TIMER_ERR_RESERVED;
    if (!s_configured[ch]) return TIMER_ERR_NOT_CONFIGURED;
    s_running[ch]=1u;return TIMER_OK;
}
TimerResult TimerManager_Stop(TimerChannel ch) {
    if ((unsigned)ch>=4u) return TIMER_ERR_ARGUMENT;
    if (ch==s_reserved) return TIMER_ERR_RESERVED;
    s_running[ch]=0u;return TIMER_OK;
}
uint32_t TimerManager_GetTickMs(void) {return 0u;}
void MockTimer_Trigger(uint32_t ch) {if (ch<4u && s_running[ch] && s_cb[ch]) s_cb[ch](s_ctx[ch]);}
int MockTimer_IsRunning(uint32_t ch) {return ch<4u && s_running[ch];}
