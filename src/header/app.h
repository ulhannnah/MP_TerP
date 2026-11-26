#ifndef MP_TERP_APP_H
#define MP_TERP_APP_H
#include <stdbool.h>
#include <stdint.h>
#include "radar_scan.h"
#include "radar_processing.h"
#include "target_selector.h"
#include "aim_controller.h"
#include "radar_renderer.h"
#include "lcd_status.h"
#include "launcher.h"

/* Board-facing operations. The integration layer chooses all GPIO, ADC,
 * FTM, LPIT and display channels. Return false on hardware failure. */
typedef struct {
    void *context;
    bool (*servo_set_angle)(void *context,int32_t angle_mdeg);
    bool (*echo_start)(void *context,uint32_t now_ms);
    bool (*echo_take)(void *context,uint32_t *distance_mm,bool *valid);
    void (*echo_update)(void *context,uint32_t now_ms);
    bool (*adc_read)(void *context,uint16_t *raw);
    bool (*aim_move)(void *context,int32_t angle_mdeg);
    /* Required for snapshot aim: cancel step timer and release coils on timeout. */
    bool (*aim_stop)(void *context);
    bool (*aim_position)(void *context,int32_t *angle_mdeg,bool *moving);
    bool (*oled_flush)(void *context);
    bool (*lcd_write)(void *context,uint8_t row,const char *text);
    void (*led_write)(void *context,bool on);
} AppHardware;

typedef struct {
    AppHardware hw;
    int32_t minimum_scan_angle_mdeg,maximum_scan_angle_mdeg,scan_step_mdeg;
    uint32_t settle_ms,echo_timeout_ms;
    uint8_t average_window;
    uint32_t background_threshold_mm;
    TargetDetectorConfig detector;
    int32_t match_angle_mdeg;
    uint32_t match_range_mm,lost_timeout_ms;
    uint8_t max_misses;
    uint16_t adc_max,adc_hysteresis;
    uint8_t operating_target_limit; /* 0: default 3, 1..5: explicit mode */
    uint32_t maximum_display_distance_mm;
    uint32_t oled_period_ms,lcd_period_ms;
    uint32_t aim_max_age_ms,aim_tolerance_mdeg;
    uint32_t simulation_ms,button_debounce_ms;
    /* Opt-in: capture target angle at request time, only indicate after aim.
     * Defaults to 0 for older virtual/legacy tests. */
    uint8_t fire_snapshot_mode;
    uint32_t fire_aim_timeout_ms;
    /* 1: Servo sweeps and Echo can be inspected separately, but the sensor
     * is NOT mechanically coupled. Do not form angular frames/targets or aim.
     * Set only in a non-coupled diagnostic build; normal App uses 0. */
    uint8_t uncoupled_diagnostic;
} AppConfig;

typedef enum {
    APP_FIRE_IDLE=0,
    APP_FIRE_AIMING=1,
    APP_FIRE_INDICATING=2,
    APP_FIRE_TIMED_OUT=3
} AppFireState;

typedef struct {
    AppConfig config;
    RadarScan scan;
    RadarProcessing processing;
    TargetSelector selector;
    AimController aim;
    RadarRendererConfig renderer;
    LcdStatus lcd;
    Launcher launcher;
    uint32_t last_oled_ms,last_lcd_ms;
    AppFireState fire_state;
    int32_t fire_snapshot_angle_mdeg;
    uint32_t fire_requested_at_ms;
    uint8_t fire_snapshot_id;
    /* Raw fixed-direction sonar diagnostics. These do not encode a bearing. */
    uint32_t diagnostic_samples;
    uint32_t diagnostic_range_mm;
    uint8_t diagnostic_valid;
    uint8_t initialized;
} App;

bool App_Init(App *app,const AppConfig *cfg,uint32_t now_ms);
void App_Update(App *app,uint32_t now_ms);
void App_RequestFire(App *app,uint32_t now_ms);
void App_RequestBackgroundCalibration(App *app);
#endif
