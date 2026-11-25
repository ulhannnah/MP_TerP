#include "timer_manager.h"
#include "mp_irq_compat.h"

/* One owner for LPIT0 and LPIT0_Ch0..Ch3_IRQHandler.
 * Users choose channel mapping during application initialization.
 */
#define LPIT_T_EN   (1u << 0)
#define LPIT_TSOI   (1u << 17) /* Timeout stop; TSOT at bit 16 is trigger-start. */

/* Debugger-visible initialization checkpoints. */
volatile uint32_t g_lpit_init_stage;
volatile uint32_t g_lpit_init_channel;
static volatile uint32_t s_tick_ms;
static volatile uint8_t s_initialized;
static volatile uint8_t s_configured[4];
static uint32_t s_clocks_per_us;
static TimerChannel s_tick_channel;
static TimerCallback s_callbacks[4];
static void *s_contexts[4];

static bool valid_channel(TimerChannel ch)
{
    return ((unsigned)ch < 4u);
}

static IRQn_Type irq_for(uint32_t ch)
{
    switch (ch) {
        case 0u: return LPIT0_Ch0_IRQn;
        case 1u: return LPIT0_Ch1_IRQn;
        case 2u: return LPIT0_Ch2_IRQn;
        default: return LPIT0_Ch3_IRQn;
    }
}

static TimerResult check_user_channel(TimerChannel channel)
{
    if (!valid_channel(channel)) return TIMER_ERR_ARGUMENT;
    if (channel == s_tick_channel) return TIMER_ERR_RESERVED;
    return TIMER_OK;
}

static void program_channel(uint32_t ch, uint32_t period_us, bool one_shot)
{
    const uint32_t mask = 1u << ch;
    LPIT0->TMR[ch].TCTRL = 0u;
    LPIT0->MIER &= ~mask;
    LPIT0->MSR = mask; /* W1C status */
    LPIT0->TMR[ch].TVAL = (period_us * s_clocks_per_us) - 1u;
    LPIT0->TMR[ch].TCTRL = one_shot ? LPIT_TSOI : 0u;
    MP_NvicClearPending(irq_for(ch));
    LPIT0->MIER |= mask;
}

TimerResult TimerManager_Init(const TimerManagerConfig *config)
{
    uint32_t ch;
    if (config == (const TimerManagerConfig *)0 ||
        config->pcc_clock_source == 0u || config->pcc_clock_source > 7u ||
        config->clock_hz < 1000000u ||
        config->clock_hz % 1000000u != 0u ||
        (config->tick_channel != TIMER_CHANNEL_NONE &&
         !valid_channel(config->tick_channel))) return TIMER_ERR_ARGUMENT;

    g_lpit_init_stage = 1u;
    g_lpit_init_channel = 0xffffffffu;
    s_initialized = 0u;
    for (ch = 0u; ch < 4u; ++ch) MP_NvicDisable(irq_for(ch));

    PCC->PCCn[PCC_LPIT_INDEX] &= ~PCC_PCCn_CGC_MASK;
    PCC->PCCn[PCC_LPIT_INDEX] =
        (PCC->PCCn[PCC_LPIT_INDEX] & ~PCC_PCCn_PCS_MASK) |
        PCC_PCCn_PCS(config->pcc_clock_source) | PCC_PCCn_CGC_MASK;

    g_lpit_init_stage = 2u;
    /* Enable the functional module clock before touching channel registers.
     * Allow at least four functional-clock cycles before further access.
     * Current board: 80MHz core / 40MHz LPIT. 32 NOPs allow ample margin.
     */
    LPIT0->MCR = LPIT_MCR_M_CEN_MASK;
    (void)LPIT0->MCR;
#if defined(__arm__) || defined(__thumb__)
    __asm volatile ("dsb" ::: "memory");
    for (volatile uint32_t i = 0u; i < 32u; ++i)
        __asm volatile ("nop");
#endif
    g_lpit_init_stage = 3u;
    LPIT0->MIER = 0u;
    g_lpit_init_stage = 4u;
    for (ch = 0u; ch < 4u; ++ch) {
        g_lpit_init_channel = ch;
        LPIT0->TMR[ch].TCTRL = 0u;
    }
    LPIT0->MSR = 0x0fu;
    g_lpit_init_stage = 5u;

    s_tick_ms = 0u;
    s_clocks_per_us = config->clock_hz / 1000000u;
    s_tick_channel = config->tick_channel;
    for (ch = 0u; ch < 4u; ++ch) {
        s_configured[ch] = 0u;
        s_callbacks[ch] = (TimerCallback)0;
        s_contexts[ch] = (void *)0;
    }
    if (s_tick_channel != TIMER_CHANNEL_NONE) {
        const uint32_t tick = (uint32_t)s_tick_channel;
        program_channel(tick, 1000u, false);
        s_configured[tick] = 1u;
    }
    g_lpit_init_stage = 6u;
    s_initialized = 1u;
    if (s_tick_channel != TIMER_CHANNEL_NONE) {
        const uint32_t tick = (uint32_t)s_tick_channel;
        MP_NvicEnable(irq_for(tick));
        LPIT0->TMR[tick].TCTRL |= LPIT_T_EN;
    }
    g_lpit_init_stage = 7u;
    return TIMER_OK;
}

TimerResult TimerManager_Configure(TimerChannel channel, uint32_t period_us, bool one_shot)
{
    const TimerResult result = check_user_channel(channel);
    const uint32_t ch = (uint32_t)channel;
    if (result != TIMER_OK) return result;
    if (s_initialized == 0u) return TIMER_ERR_NOT_INITIALIZED;
    if (period_us == 0u || period_us > UINT32_MAX / s_clocks_per_us)
        return TIMER_ERR_RANGE;

    MP_NvicDisable(irq_for(ch));
    s_configured[ch] = 0u;
    program_channel(ch, period_us, one_shot);
    s_configured[ch] = 1u;
    MP_NvicEnable(irq_for(ch));
    return TIMER_OK;
}

TimerResult TimerManager_RegisterCallback(TimerChannel channel, TimerCallback callback, void *context)
{
    const TimerResult result = check_user_channel(channel);
    const uint32_t ch = (uint32_t)channel;
    if (result != TIMER_OK) return result;
    if (s_initialized == 0u) return TIMER_ERR_NOT_INITIALIZED;
    MP_NvicDisable(irq_for(ch));
    if ((LPIT0->TMR[ch].TCTRL & LPIT_T_EN) != 0u) {
        MP_NvicEnable(irq_for(ch));
        return TIMER_ERR_BUSY;
    }
    s_callbacks[ch] = callback;
    s_contexts[ch] = context;
    if (s_configured[ch] != 0u) MP_NvicEnable(irq_for(ch));
    return TIMER_OK;
}

TimerResult TimerManager_Start(TimerChannel channel)
{
    const TimerResult result = check_user_channel(channel);
    const uint32_t ch = (uint32_t)channel;
    if (result != TIMER_OK) return result;
    if (s_initialized == 0u) return TIMER_ERR_NOT_INITIALIZED;
    if (s_configured[ch] == 0u) return TIMER_ERR_NOT_CONFIGURED;
    MP_NvicDisable(irq_for(ch));
    LPIT0->TMR[ch].TCTRL &= ~LPIT_T_EN;
    LPIT0->MSR = 1u << ch;
    MP_NvicClearPending(irq_for(ch));
    LPIT0->TMR[ch].TCTRL |= LPIT_T_EN;
    MP_NvicEnable(irq_for(ch));
    return TIMER_OK;
}

TimerResult TimerManager_Stop(TimerChannel channel)
{
    const TimerResult result = check_user_channel(channel);
    const uint32_t ch = (uint32_t)channel;
    if (result != TIMER_OK) return result;
    if (s_initialized == 0u) return TIMER_ERR_NOT_INITIALIZED;
    MP_NvicDisable(irq_for(ch));
    LPIT0->TMR[ch].TCTRL &= ~LPIT_T_EN;
    LPIT0->MSR = 1u << ch;
    MP_NvicClearPending(irq_for(ch));
    if (s_configured[ch] != 0u) MP_NvicEnable(irq_for(ch));
    return TIMER_OK;
}

uint32_t TimerManager_GetTickMs(void) { return s_tick_ms; }

static void handle_irq(uint32_t ch)
{
    const uint32_t mask = 1u << ch;
    if ((LPIT0->MSR & mask) == 0u) return;
    LPIT0->MSR = mask;
    if (s_tick_channel != TIMER_CHANNEL_NONE &&
        ch == (uint32_t)s_tick_channel) {
        ++s_tick_ms;
    } else if (s_callbacks[ch] != (TimerCallback)0) {
        s_callbacks[ch](s_contexts[ch]);
    }
}

void LPIT0_Ch0_IRQHandler(void) { handle_irq(0u); }
void LPIT0_Ch1_IRQHandler(void) { handle_irq(1u); }
void LPIT0_Ch2_IRQHandler(void) { handle_irq(2u); }
void LPIT0_Ch3_IRQHandler(void) { handle_irq(3u); }
