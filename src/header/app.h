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
} AppConfig;

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
    uint8_t initialized;
} App;

bool App_Init(App *app,const AppConfig *cfg,uint32_t now_ms);
void App_Update(App *app,uint32_t now_ms);
void App_RequestFire(App *app,uint32_t now_ms);
void App_RequestBackgroundCalibration(App *app);
#endif
