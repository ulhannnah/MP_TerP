#include "cycle_profiler.h"
#include <stddef.h>

/* Cortex-M4 DWT registers are architectural PPB registers. The S32K SDK
 * may expose s32_core_cm4.h rather than CMSIS CoreDebug/DWT objects.
 * Use architectural register addresses to avoid that SDK dependency.
 * ARMv7-M: DEMCR 0xE000EDFC, DWT_CTRL 0xE0001000, CYCCNT 0xE0001004.
 */
#if defined(__arm__) || defined(__thumb__)
#define MP_CYCLE_HW 1
#define MP_DEMCR (*(volatile uint32_t *)(uintptr_t)0xE000EDFCu)
#define MP_DWT_CTRL (*(volatile uint32_t *)(uintptr_t)0xE0001000u)
#define MP_DWT_CYCCNT (*(volatile uint32_t *)(uintptr_t)0xE0001004u)
#define MP_DEMCR_TRCENA (1u << 24)
#define MP_DWT_CYCCNTENA (1u << 0)
#else
#define MP_CYCLE_HW 0
#endif

bool CycleProfiler_Enable(void)
{
#if MP_CYCLE_HW
    MP_DEMCR |= MP_DEMCR_TRCENA;
    MP_DWT_CYCCNT = 0u;
    MP_DWT_CTRL |= MP_DWT_CYCCNTENA;
    return (MP_DWT_CTRL & MP_DWT_CYCCNTENA) != 0u;
#else
    return false;
#endif
}

static uint32_t now_cycles(void)
{
#if MP_CYCLE_HW
    return MP_DWT_CYCCNT;
#else
    return 0u;
#endif
}

void CycleProfiler_Reset(CycleProfiler *p)
{
    if (p == NULL) return;
    p->start = 0u;
    p->last = 0u;
    p->maximum = 0u;
    p->total = 0u;
    p->count = 0u;
}

void CycleProfiler_Begin(CycleProfiler *p)
{
    if (p != NULL) p->start = now_cycles();
}

void CycleProfiler_End(CycleProfiler *p)
{
    uint32_t elapsed;
    if (p == NULL) return;
    elapsed = now_cycles() - p->start;
    p->last = elapsed;
    p->total += elapsed;
    if (elapsed > p->maximum) p->maximum = elapsed;
    if (p->count < UINT32_MAX) ++p->count;
}

uint32_t CycleProfiler_Average(const CycleProfiler *p)
{
    return (p == NULL || p->count == 0u)
        ? 0u : (uint32_t)(p->total / p->count);
}
