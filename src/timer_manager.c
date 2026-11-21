#include "timer_manager.h"
#include "device_registers.h"

/* Sole owner of LPIT0 and its IRQ handlers.
 * Requires SPLLDIV2 = 40 MHz from clocks_and_modes.c.
 */
#define LPIT_CLOCKS_PER_US  (40u)
#define LPIT_MAX_US         (UINT32_MAX / LPIT_CLOCKS_PER_US)
#define LPIT_T_EN           (1u << 0)
#define LPIT_TSOT           (1u << 16)
#define DISPLAY_OLED_MS     (100u)
#define DISPLAY_LCD_MS      (200u)

static volatile uint32_t s_tick_ms;
static volatile uint8_t s_oled_due;
static volatile uint8_t s_lcd_due;
static volatile uint8_t s_initialized;
static TimerCallback s_callbacks[2];
static void *s_contexts[2];

static IRQn_Type channel_irq(uint32_t ch)
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
    if ((int)channel < 0 || (int)channel > 3) return TIMER_ERR_ARGUMENT;
    if (channel == TIMER_CH_SYSTEM || channel == TIMER_CH_RESERVED) return TIMER_ERR_RESERVED;
    return TIMER_OK;
}

static void configure_raw(uint32_t channel, uint32_t period_us, bool one_shot)
{
    LPIT0->TMR[channel].TCTRL = 0u;
    LPIT0->MIER &= ~(1u << channel);
    LPIT0->MSR = 1u << channel;
    LPIT0->TMR[channel].TVAL = (period_us * LPIT_CLOCKS_PER_US) - 1u;
    LPIT0->TMR[channel].TCTRL = one_shot ? LPIT_TSOT : 0u;
    LPIT0->MIER |= (1u << channel);
    NVIC_ClearPendingIRQ(channel_irq(channel));
    NVIC_EnableIRQ(channel_irq(channel));
}

TimerResult TimerManager_Init(void)
{
    uint32_t ch;
    s_initialized = 0u;
    for (ch = 0u; ch < 4u; ++ch) {
        NVIC_DisableIRQ(channel_irq(ch));
    }

    PCC->PCCn[PCC_LPIT_INDEX] &= ~PCC_PCCn_CGC_MASK;
    PCC->PCCn[PCC_LPIT_INDEX] = PCC_PCCn_PCS(6) | PCC_PCCn_CGC_MASK;
    LPIT0->MCR = 0u;
    LPIT0->MIER = 0u;
    LPIT0->MCR = LPIT_MCR_M_CEN_MASK;
    for (ch = 0u; ch < 4u; ++ch) {
        LPIT0->TMR[ch].TCTRL = 0u;
    }
    LPIT0->MSR = 0x0Fu;
    s_tick_ms = 0u;
    s_oled_due = 0u;
    s_lcd_due = 0u;
    for (ch = 0u; ch < 2u; ++ch) {
        s_callbacks[ch] = (TimerCallback)0;
        s_contexts[ch] = (void *)0;
    }
    configure_raw(2u, 1000u, false);
    s_initialized = 1u;
    LPIT0->TMR[2].TCTRL |= LPIT_T_EN;
    return TIMER_OK;
}

TimerResult TimerManager_Configure(TimerChannel channel, uint32_t period_us, bool one_shot)
{
    TimerResult r = check_user_channel(channel);
    if (r != TIMER_OK) return r;
    if (s_initialized == 0u) return TIMER_ERR_NOT_INITIALIZED;
    if (period_us == 0u || period_us > LPIT_MAX_US) return TIMER_ERR_RANGE;
    NVIC_DisableIRQ(channel_irq((uint32_t)channel));
    configure_raw((uint32_t)channel, period_us, one_shot);
    return TIMER_OK;
}

TimerResult TimerManager_RegisterCallback(TimerChannel channel, TimerCallback callback, void *context)
{
    TimerResult r = check_user_channel(channel);
    if (r != TIMER_OK) return r;
    if (s_initialized == 0u) return TIMER_ERR_NOT_INITIALIZED;
    if ((LPIT0->TMR[(uint32_t)channel].TCTRL & LPIT_T_EN) != 0u) return TIMER_ERR_RESERVED;
    s_callbacks[(uint32_t)channel] = callback;
    s_contexts[(uint32_t)channel] = context;
    return TIMER_OK;
}

TimerResult TimerManager_Start(TimerChannel channel)
{
    TimerResult r = check_user_channel(channel);
    if (r != TIMER_OK) return r;
    if (s_initialized == 0u) return TIMER_ERR_NOT_INITIALIZED;
    if (LPIT0->TMR[(uint32_t)channel].TVAL == 0u) return TIMER_ERR_RANGE;
    LPIT0->MSR = (1u << (uint32_t)channel);
    LPIT0->TMR[(uint32_t)channel].TCTRL |= LPIT_T_EN;
    return TIMER_OK;
}

TimerResult TimerManager_Stop(TimerChannel channel)
{
    TimerResult r = check_user_channel(channel);
    if (r != TIMER_OK) return r;
    if (s_initialized == 0u) return TIMER_ERR_NOT_INITIALIZED;
    LPIT0->TMR[(uint32_t)channel].TCTRL &= ~LPIT_T_EN;
    LPIT0->MSR = (1u << (uint32_t)channel);
    return TIMER_OK;
}

uint32_t TimerManager_GetTickMs(void) { return s_tick_ms; }

static bool take_flag(volatile uint8_t *flag)
{
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    bool result = (*flag != 0u);
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
        if ((s_tick_ms % DISPLAY_OLED_MS) == 0u) s_oled_due = 1u;
        if ((s_tick_ms % DISPLAY_LCD_MS) == 0u) s_lcd_due = 1u;
    }
}

void LPIT0_Ch3_IRQHandler(void)
{
    if ((LPIT0->MSR & 8u) != 0u) LPIT0->MSR = 8u;
}
