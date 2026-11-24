/*
 * Optional step-by-step board bring-up entry point.
 *
 * Build flags (S32DS project Symbols):
 *   MP_TERP_STAGE_MAIN
 *   MP_TERP_STAGE=1   clock + LPIT only
 *   MP_TERP_STAGE=2   servo PWM only
 *   MP_TERP_STAGE=3   ultrasonic Trigger/Echo only
 *   MP_TERP_STAGE=4   virtual echo + production App (NO peripherals)
 *   MP_TERP_STAGE=5   full BoardSystem (REQUIRES geometry confirmation)
 *
 * No symbol -> original safe smoke main.c.
 * DO NOT define MP_TERP_USE_EXAMPLE_MAIN simultaneously.
 *
 * Stages 1,2,3,5 require the user-defined *verified* board mapping:
 *   bool MP_TERP_GetBoardConfig(BoardSystemConfig *cfg);
 * The repository intentionally does not guess GPIO/FTM/LPIT pins.
 */
#if defined(MP_TERP_STAGE_MAIN)
#ifndef MP_TERP_STAGE
#error "Specify MP_TERP_STAGE=1,2,3,4 or 5"
#endif
#if MP_TERP_STAGE < 1 || MP_TERP_STAGE > 5
#error "MP_TERP_STAGE must be 1 through 5"
#endif

#include <stdbool.h>
#include <stdint.h>
#include "board_system.h"
#include "clocks_and_modes.h"

/* Always visible in the S32DS debugger; no blocking serial print required. */
volatile uint32_t g_stage_state;       /* 0 boot, 1 ready, 2 working, 0xDEAD failed */
volatile uint32_t g_stage_error;       /* nonzero means diagnostic failure */
volatile uint32_t g_stage_tick_ms;
volatile uint32_t g_stage_samples;
volatile uint32_t g_stage_valid;
volatile uint32_t g_stage_invalid;
volatile uint32_t g_stage_last_mm;
volatile uint32_t g_stage_frames;
volatile uint32_t g_stage_candidates;
volatile uint32_t g_stage_active_tracks;
volatile uint32_t g_stage_selected_id;
volatile int32_t g_stage_servo_request_mdeg; /* Stage 2: change in debugger to move */
volatile int32_t g_stage_servo_applied_mdeg;

static void stage_fault(uint32_t error)
{
    g_stage_error=error;
    g_stage_state=0xDEADu;
}

#if MP_TERP_STAGE != 4
extern bool MP_TERP_GetBoardConfig(BoardSystemConfig *cfg);
static BoardSystemConfig s_cfg;
static bool init_cfg_and_clock(void)
{
    if(!MP_TERP_GetBoardConfig(&s_cfg))return false;
    SOSC_init_8MHz();
    SPLL_init_160MHz();
    NormalRUNmode_80MHz();
    return TimerManager_Init(&s_cfg.timer)==TIMER_OK;
}
#endif

#if MP_TERP_STAGE == 2
static Servo s_servo;
#elif MP_TERP_STAGE == 3
static UltrasonicPort s_ultrasonic;
#elif MP_TERP_STAGE == 5
static BoardSystem s_board;
#elif MP_TERP_STAGE == 4
static App s_app;
static int32_t s_bench_angle;
static uint32_t s_bench_sweep;
static uint32_t s_bench_pending_echo;
static bool bench_servo(void *context,int32_t angle)
{
    (void)context;
    if(s_bench_angle==150000 && angle<150000)++s_bench_sweep;
    s_bench_angle=angle;
    return true;
}
static bool bench_begin(void *context,uint32_t now)
{
    (void)context;
    (void)now;
    s_bench_pending_echo=1u;
    return true;
}
static bool bench_take(void *context,uint32_t *mm,bool *valid)
{
    (void)context;
    if(!s_bench_pending_echo)return false;
    s_bench_pending_echo=0u;
    *valid=true;
    *mm=3000u;
    if(s_bench_sweep>0u) {
        if(s_bench_angle>=60000 && s_bench_angle<=75000)*mm=1200u;
        if(s_bench_angle>=105000 && s_bench_angle<=120000)*mm=1800u;
    }
    ++g_stage_samples;
    ++g_stage_valid;
    g_stage_last_mm=*mm;
    return true;
}
static bool bench_adc(void *context,uint16_t *raw)
{
    (void)context;
    *raw=0u;
    return true;
}
static bool bench_aim(void *context,int32_t angle)
{
    (void)context;
    (void)angle;
    /* No motor drive on synthetic bench. */
    return true;
}
static bool bench_position(void *context,int32_t *angle,bool *moving)
{
    (void)context;
    *angle=0;
    *moving=false;
    return true;
}
static void bench_led(void *context,bool on){(void)context;(void)on;}
static bool init_bench(void)
{
    AppConfig cfg={0};
    cfg.hw.servo_set_angle=bench_servo;
    cfg.hw.echo_start=bench_begin;
    cfg.hw.echo_take=bench_take;
    cfg.hw.adc_read=bench_adc;
    cfg.hw.aim_move=bench_aim;
    cfg.hw.aim_position=bench_position;
    cfg.hw.led_write=bench_led;
    /* No OLED/LCD or hardware IO on this stage. */
    cfg.minimum_scan_angle_mdeg=30000;
    cfg.maximum_scan_angle_mdeg=150000;
    cfg.scan_step_mdeg=5000;
    cfg.settle_ms=10u;
    cfg.echo_timeout_ms=30u;
    cfg.average_window=1u;
    cfg.background_threshold_mm=150u;
    cfg.detector.max_range_difference_mm=250u;
    cfg.detector.max_angle_gap_mdeg=11000;
    cfg.detector.min_cluster_samples=2u;
    cfg.match_angle_mdeg=20000;
    cfg.match_range_mm=500u;
    cfg.lost_timeout_ms=1000u;
    cfg.max_misses=2u;
    cfg.adc_max=4095u;
    cfg.adc_hysteresis=20u;
    cfg.operating_target_limit=3u;
    cfg.maximum_display_distance_mm=4000u;
    cfg.oled_period_ms=100u;
    cfg.lcd_period_ms=100u;
    cfg.aim_max_age_ms=1000u;
    cfg.aim_tolerance_mdeg=2000;
    cfg.simulation_ms=40u;
    cfg.button_debounce_ms=10u;
    s_bench_angle=30000;
    s_bench_sweep=0u;
    s_bench_pending_echo=0u;
    return App_Init(&s_app,&cfg,0u);
}
#endif

/*
 * The startup project's actual FTM channel-pair vector must call this hook
 * ONLY for the configured Echo channel (e.g. via a user-defined IRQ handler).
 * Defining the hook does NOT register a vector or enable its NVIC interrupt.
 */
void MP_TERP_StageEchoCaptureIrq(void)
{
#if MP_TERP_STAGE == 3
    UltrasonicPort_OnCaptureIrq(&s_ultrasonic);
#elif MP_TERP_STAGE == 5
    BoardSystem_OnEchoCaptureIrq(&s_board);
#endif
}

void MP_TERP_StageButtonPortIrq(void)
{
#if MP_TERP_STAGE == 5
    BoardSystem_OnFireButtonPortIrq(&s_board);
#endif
}

int main(void)
{
    g_stage_state=0u;
    g_stage_error=0u;
    g_stage_tick_ms=0u;
    g_stage_samples=0u;
    g_stage_valid=0u;
    g_stage_invalid=0u;
    g_stage_last_mm=0u;
    g_stage_frames=0u;
    g_stage_candidates=0u;
    g_stage_active_tracks=0u;
    g_stage_selected_id=0u;
    g_stage_servo_request_mdeg=0;
    g_stage_servo_applied_mdeg=0;

#if MP_TERP_STAGE == 4
    /* Synthetic bench: does not configure clocks or touch peripheral registers. */
    if(!init_bench())stage_fault(40u);
    else g_stage_state=1u;
#elif MP_TERP_STAGE == 5
#ifndef MP_TERP_CONFIRM_COUPLED_GEOMETRY
    /*
     * A fixed ultrasonic sensor CANNOT be labelled with a separate
     * servo's angle: do not run physical radar in that configuration.
     */
    stage_fault(50u);
#else
    if(!MP_TERP_GetBoardConfig(&s_cfg))stage_fault(51u);
    else if(!BoardSystem_Init(&s_board,&s_cfg))stage_fault(52u);
    else g_stage_state=1u;
#endif
#else
    if(!init_cfg_and_clock())stage_fault(10u);
    else {
#if MP_TERP_STAGE == 2
        if(Servo_Init(&s_servo,&s_cfg.servo)!=SERVO_OK)stage_fault(20u);
        else {
            /* Hold the configured initial angle, no automatic motion. */
            g_stage_servo_applied_mdeg=s_cfg.servo.initial_angle_mdeg;
            g_stage_servo_request_mdeg=s_cfg.servo.initial_angle_mdeg;
            g_stage_state=1u;
        }
#elif MP_TERP_STAGE == 3
        if(!UltrasonicPort_Init(&s_ultrasonic,&s_cfg.ultrasonic))
            stage_fault(30u);
        else g_stage_state=1u;
#else
        g_stage_state=1u;
#endif
    }
#endif

    uint32_t last_trigger=0u;
    uint32_t virtual_time_ms=0u;
    for(;;) {
        if(g_stage_state==0xDEADu)continue;
#if MP_TERP_STAGE == 4
        /* Deterministic 1ms simulation time, not hardware real-time. */
        App_Update(&s_app,virtual_time_ms++);
        g_stage_tick_ms=virtual_time_ms;
        g_stage_frames=s_app.processing.display_frame.frame_number;
        g_stage_candidates=s_app.processing.candidate_count;
        g_stage_selected_id=TargetSelector_GetId(&s_app.selector);
        g_stage_active_tracks=0u;
        for(uint8_t i=0u;i<s_app.processing.tracker.active_limit;i++) {
            const RadarTarget *t=&s_app.processing.tracker.targets[i];
            if(t->state==TARGET_NEW||t->state==TARGET_TRACKING)
                ++g_stage_active_tracks;
        }
#elif MP_TERP_STAGE == 5
        BoardSystem_Update(&s_board);
        g_stage_tick_ms=TimerManager_GetTickMs();
        g_stage_frames=s_board.app.processing.display_frame.frame_number;
        g_stage_candidates=s_board.app.processing.candidate_count;
        g_stage_selected_id=TargetSelector_GetId(&s_board.app.selector);
#elif MP_TERP_STAGE == 2
        g_stage_tick_ms=TimerManager_GetTickMs();
        int32_t request=g_stage_servo_request_mdeg;
        if(request!=g_stage_servo_applied_mdeg) {
            if(Servo_SetAngle(&s_servo,request)==SERVO_OK)
                g_stage_servo_applied_mdeg=request;
            else {
                g_stage_servo_request_mdeg=g_stage_servo_applied_mdeg;
                g_stage_error=21u;
            }
        }
#elif MP_TERP_STAGE == 3
        uint32_t now=TimerManager_GetTickMs();
        g_stage_tick_ms=now;
        UltrasonicPort_Update(&s_ultrasonic,now);
        uint32_t mm=0u;
        bool valid=false;
        if(UltrasonicPort_Take(&s_ultrasonic,&mm,&valid)) {
            ++g_stage_samples;
            if(valid)++g_stage_valid;
            else ++g_stage_invalid;
            g_stage_last_mm=mm;
        }
        if(s_ultrasonic.sensor.state==US_IDLE &&
           (uint32_t)(now-last_trigger)>=100u) {
            if(UltrasonicPort_Start(&s_ultrasonic,now))
                last_trigger=now;
            else g_stage_error=31u;
        }
#else
        g_stage_tick_ms=TimerManager_GetTickMs();
#endif
        g_stage_state=2u;
    }
}
#endif /* MP_TERP_STAGE_MAIN */
