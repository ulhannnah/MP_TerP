#include "timer_manager.h"
#include "device_registers.h"

/* This module exclusively owns LPIT0, CH0..CH3 and their IRQ handlers.
 * Clock precondition: clocks_and_modes.c set SPLLDIV2 to 40 MHz.
 */
#define LPIT_CLOCKS_PER_US 40u
#define LPIT_MAX_US (UINT32_MAX / LPIT_CLOCKS_PER_US)
#define LPIT_T_EN (1u << 0)
#define LPIT_TSOI (1u << 17) /* Stop after interrupt. TSOT (bit 16) is NOT one-shot. */
#define OLED_PERIOD_MS 100u
#define LCD_PERIOD_MS 200u

static volatile uint32_t s_tick_ms;
static volatile uint8_t s_oled_due;
static volatile uint8_t s_lcd_due;
static volatile uint8_t s_initialized;
static volatile uint8_t s_configured[2];
static TimerCallback s_callbacks[2];
static void *s_contexts[2];

static IRQn_Type irq_for(uint32_t channel)
{
    switch (channel) {
        case 0u: return LPIT0_Ch0_IRQn;
        case 1u: return LPIT0_Ch1_IRQn;
        case 2u: return LPIT0_Ch2_IRQn;
        default: return LPIT0_Ch3_IRQn;
    }
}

static TimerResult check_user_channel(TimerChannel channel)
{
    if ((int)channel < 0 || (int)channel > 3) return TIMER_ERR_ARGUMENT;
    if (channel >= TIMER_CH_SYSTEM) return TIMER_ERR_RESERVED;
    return TIMER_OK;
}

static void configure_channel(uint32_t channel, uint32_t period_us, bool one_shot)
{
    const uint32_t mask = 1u << channel;
    LPIT0->TMR[channel].TCTRL = 0u;
    LPIT0->MIER &= ~mask;
    LPIT0->MSR = mask; /* W1C */
    LPIT0->TMR[channel].TVAL = period_us * LPIT_CLOCKS_PER_US - 1u;
    LPIT0->TMR[channel].TCTRL = one_shot ? LPIT_TSOI : 0u;
    NVIC_ClearPendingIRQ(irq_for(channel));
    LPIT0->MIER |= mask;
    NVIC_EnableIRQ(irq_for(channel));
}

TimerResult TimerManager_Init(void)
{
    uint32_t ch;
    s_initialized = 0u;
    for (ch = 0u; ch < 4u; ++ch) NVIC_DisableIRQ(irq_for(ch));

    PCC->PCCn[PCC_LPIT_INDEX] &= ~PCC_PCCn_CGC_MASK;
    PCC->PCCn[PCC_LPIT_INDEX] = PCC_PCCn_PCS(6) | PCC_PCCn_CGC_MASK;
    LPIT0->MCR = 0u;
    LPIT0->MIER = 0u;
    LPIT0->MCR = LPIT_MCR_M_CEN_MASK;
    for (ch = 0u; ch < 4u; ++ch) LPIT0->TMR[ch].TCTRL = 0u;
    LPIT0->MSR = 0x0fu;

    s_tick_ms = 0u;
    s_oled_due = 0u;
    s_lcd_due = 0u;
    for (ch = 0u; ch < 2u; ++ch) {
        s_configured[ch] = 0u;
        s_callbacks[ch] = (TimerCallback)0;
        s_contexts[ch] = (void *)0;
    }
    configure_channel(2u, 1000u, false);
    s_initialized = 1u;
    LPIT0->TMR[2].TCTRL |= LPIT_T_EN;
    return TIMER_OK;
}

TimerResult TimerManager_Configure(TimerChannel channel, uint32_t period_us, bool one_shot)
{
    TimerResult result = check_user_channel(channel);
    if (result != TIMER_OK) return result;
    if (s_initialized == 0u) return TIMER_ERR_NOT_INITIALIZED;
    if (period_us == 0u || period_us > LPIT_MAX_US) return TIMER_ERR_RANGE;
    NVIC_DisableIRQ(irq_for((uint32_t)channel));
    configure_channel((uint32_t)channel, period_us, one_shot);
    s_configured[(uint32_t)channel] = 1u;
    return TIMER_OK;
}

TimerResult TimerManager_RegisterCallback(TimerChannel channel, TimerCallback callback, void *context)
{
    TimerResult result = check_user_channel(channel);
    if (result != TIMER_OK) return result;
    if (s_initialized == 0u) return TIMER_ERR_NOT_INITIALIZED;
    if ((LPIT0->TMR[(uint32_t)channel].TCTRL & LPIT_T_EN) != 0u) return TIMER_ERR_BUSY;
    s_callbacks[(uint32_t)channel] = callback;
    s_contexts[(uint32_t)channel] = context;
    return TIMER_OK;
}

TimerResult TimerManager_Start(TimerChannel channel)
{
    TimerResult result = check_user_channel(channel);
    const uint32_t ch = (uint32_t)channel;
    if (result != TIMER_OK) return result;
    if (s_initialized == 0u) return TIMER_ERR_NOT_INITIALIZED;
    if (s_configured[ch] == 0u) return TIMER_ERR_NOT_CONFIGURED;
    /* Clear T_EN before setting it so a completed one-shot can rearm. */
    LPIT0->TMR[ch].TCTRL &= ~LPIT_T_EN;
    LPIT0->MSR = 1u << ch;
    NVIC_ClearPendingIRQ(irq_for(ch));
    LPIT0->TMR[ch].TCTRL |= LPIT_T_EN;
    return TIMER_OK;
}

TimerResult TimerManager_Stop(TimerChannel channel)
{
    TimerResult result = check_user_channel(channel);
    const uint32_t ch = (uint32_t)channel;
    if (result != TIMER_OK) return result;
    if (s_initialized == 0u) return TIMER_ERR_NOT_INITIALIZED;
    LPIT0->TMR[ch].TCTRL &= ~LPIT_T_EN;
    LPIT0->MSR = 1u << ch;
    NVIC_ClearPendingIRQ(irq_for(ch));
    return TIMER_OK;
}

uint32_t TimerManager_GetTickMs(void) { return s_tick_ms; }

static bool take_flag(volatile uint8_t *flag)
{
    const uint32_t primask = __get_PRIMASK();
    bool result;
    __disable_irq();
    result = (*flag != 0u);
    *flag = 0u;
    if ((primask & 1u) == 0u) __enable_irq();
    return result;
}

bool TimerManager_TakeOledRequest(void) { return take_flag(&s_oled_due); }
bool TimerManager_TakeLcdRequest(void) { return take_flag(&s_lcd_due); }

void LPIT0_Ch0_IRQHandler(void)
{
    if ((LPIT0->MSR & 1u) != 0u) {
        LPIT0->MSR = 1u;
        if (s_callbacks[0] != (TimerCallback)0) s_callbacks[0](s_contexts[0]);
    }
}

void LPIT0_Ch1_IRQHandler(void)
{
    if ((LPIT0->MSR & 2u) != 0u) {
        LPIT0->MSR = 2u;
        if (s_callbacks[1] != (TimerCallback)0) s_callbacks[1](s_contexts[1]);
    }
}

void LPIT0_Ch2_IRQHandler(void)
{
    if ((LPIT0->MSR & 4u) != 0u) {
        LPIT0->MSR = 4u;
        ++s_tick_ms;
        if ((s_tick_ms % OLED_PERIOD_MS) == 0u) s_oled_due = 1u;
        if ((s_tick_ms % LCD_PERIOD_MS) == 0u) s_lcd_due = 1u;
    }
}

void LPIT0_Ch3_IRQHandler(void)
{
    if ((LPIT0->MSR & 8u) != 0u) LPIT0->MSR = 8u;
}
