/* Default main remains the no-peripheral smoke test.
 * Set MP_TERP_STAGE_MAIN to use main_stage.c instead (never both mains).
 */
#if !defined(MP_TERP_STAGE_MAIN) && !defined(MP_TERP_ALL_TEST_MAIN)
/*
 * MP_TerP / S32K144 SDK compile-and-link smoke test.
 *
 * This file intentionally does not touch peripherals. No motor, servo, LED,
 * trigger, or ultrasonic hardware is started. Break at MP_TERP_TestMain_Run()
 * or inspect g_mp_terp_smoke after running in S32 Design Studio.
 *
 * Compile exactly ONE main.c in the S32DS target. The repository's
 * main_example.c has a guarded main (disabled unless its macro is defined).
 */
#include <stdbool.h>
#include <stdint.h>
#include "board_system.h"
#include "radar_scan.h"
#include "radar_processing.h"
#include "target_selector.h"
#include "task_scheduler.h"
#include "dsp_fir.h"
#include "coordinate_math.h"
#include "cycle_profiler.h"

#define SMOKE_BOOT_SIGNATURE  (0x4D505450u) /* 'MPTP' */
#define SMOKE_PASS_SIGNATURE  (0x50415353u) /* 'PASS' */
#define SMOKE_FAIL_SIGNATURE  (0x4641494Cu) /* 'FAIL' */

/* Volatile values are visible in the S32DS debugger.
 * result=0 means all non-hardware smoke assertions passed.
 * A nonzero result points to the corresponding test step.
 */
volatile uint32_t g_mp_terp_smoke_signature;
volatile uint32_t g_mp_terp_smoke_result;
volatile uint32_t g_mp_terp_smoke_stage;

/* Avoid large objects on the Cortex-M4 stack. */
static RadarProcessing s_processing;
static TaskScheduler s_scheduler;
static DSPFIR s_fir;
static TargetSelector s_selector;

static void smoke_task(void *context, uint32_t now_ms)
{
    volatile uint32_t *counter = (volatile uint32_t *)context;
    (void)now_ms;
    ++(*counter);
}

static volatile uint32_t s_scheduler_calls;

static uint32_t MP_TERP_TestMain_Run(void)
{
    const TargetDetectorConfig detector = { 150u, 40000, 1u };
    const int16_t fir_coeff[2] = { 16384, 16384 };
    Coordinate2D coordinate;
    int16_t fir_value;

    g_mp_terp_smoke_stage = 1u;
    if (!RadarProcessing_Init(&s_processing,
                              30000, 150000, 30000,
                              2u, 100u, &detector,
                              40000, 500u, 1000u, 2u))
        return 1u;

    g_mp_terp_smoke_stage = 2u;
    if (!TargetSelector_Init(&s_selector, 4095u, 20u))
        return 2u;
    if (TargetSelector_Update(&s_selector, 4095u) != 5u)
        return 3u;

    g_mp_terp_smoke_stage = 3u;
    TaskScheduler_Init(&s_scheduler);
    if (!TaskScheduler_Add(&s_scheduler, smoke_task,
                           (void *)&s_scheduler_calls, 10u, 0u))
        return 4u;
    TaskScheduler_Run(&s_scheduler, 0u);
    TaskScheduler_Run(&s_scheduler, 5u);
    TaskScheduler_Run(&s_scheduler, 10u);
    if (s_scheduler_calls != 2u)
        return 5u;

    g_mp_terp_smoke_stage = 4u;
    if (!DSPFIR_Init(&s_fir, fir_coeff, 2u))
        return 6u;
    fir_value = DSPFIR_ProcessReference(&s_fir, 1000);
    if (fir_value != 500)
        return 7u;

    g_mp_terp_smoke_stage = 5u;
    if (!Coordinate_PolarToCartesian(0, 1000.0f, &coordinate))
        return 8u;
    if (coordinate.x_mm < 999.0f || coordinate.x_mm > 1001.0f ||
        coordinate.y_mm < -1.0f || coordinate.y_mm > 1.0f)
        return 9u;

    /* This is an API/link smoke test. Do not initialize any hardware here. */
    g_mp_terp_smoke_stage = 6u;
    return 0u;
}

int main(void)
{
    uint32_t result;

    g_mp_terp_smoke_signature = SMOKE_BOOT_SIGNATURE;
    g_mp_terp_smoke_result = UINT32_MAX;
    g_mp_terp_smoke_stage = 0u;

    result = MP_TERP_TestMain_Run();
    g_mp_terp_smoke_result = result;
    g_mp_terp_smoke_signature =
        (result == 0u) ? SMOKE_PASS_SIGNATURE : SMOKE_FAIL_SIGNATURE;

    /* Keep variables and debugger session alive; no peripherals are driven. */
    for (;;) {
        __asm volatile ("nop");
    }
}

#endif /* default smoke main */
