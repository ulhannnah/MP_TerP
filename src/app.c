#include "app.h"
#include "oled.h"
#include <stddef.h>
#include <stdint.h>

static bool servo_cb(void *ctx,int32_t angle) {
    App *app=(App *)ctx;
    return app->config.hw.servo_set_angle(app->config.hw.context,angle);
}
static bool echo_start_cb(void *ctx,uint32_t now){
    App *app=(App *)ctx;
    return app->config.hw.echo_start(app->config.hw.context,now);
}
static bool echo_take_cb(void *ctx,uint32_t *distance,bool *valid){
    App *app=(App *)ctx;
    return app->config.hw.echo_take(app->config.hw.context,distance,valid);
}
static bool aim_cb(void *ctx,int32_t angle){
    App *app=(App *)ctx;
    return app->config.hw.aim_move(app->config.hw.context,angle);
}
static bool lcd_cb(void *ctx,uint8_t row,const char *text){
    App *app=(App *)ctx;
    return app->config.hw.lcd_write(app->config.hw.context,row,text);
}
static void led_cb(void *ctx,bool on){
    App *app=(App *)ctx;
    app->config.hw.led_write(app->config.hw.context,on);
}
bool App_Init(App *app,const AppConfig *cfg,uint32_t now){
    RadarScanConfig sc;
    AimControllerConfig ac;
    LcdStatusConfig lc;
    LauncherConfig launcher;
    uint8_t operating_limit;
    if(!app||!cfg||!cfg->hw.servo_set_angle||!cfg->hw.echo_start||
       !cfg->hw.echo_take||!cfg->hw.aim_move||!cfg->hw.adc_read||
       !cfg->hw.aim_position||!cfg->hw.led_write||
       cfg->oled_period_ms==0u||cfg->lcd_period_ms==0u||
       cfg->aim_tolerance_mdeg<0||cfg->maximum_display_distance_mm==0u)
        return false;
    operating_limit = cfg->operating_target_limit == 0u ? 3u : cfg->operating_target_limit;
    if(operating_limit > TRACKER_MAX_TARGETS)return false;
    app->initialized=0u;
    app->config=*cfg;
    app->config.operating_target_limit=operating_limit;
    sc.min_angle_mdeg=cfg->minimum_scan_angle_mdeg;
    sc.max_angle_mdeg=cfg->maximum_scan_angle_mdeg;
    sc.step_mdeg=cfg->scan_step_mdeg;sc.settle_ms=cfg->settle_ms;
    sc.echo_timeout_ms=cfg->echo_timeout_ms;
    sc.move=servo_cb;sc.trigger=echo_start_cb;sc.take=echo_take_cb;sc.context=app;
    if(!RadarScan_Init(&app->scan,&sc)||
       !RadarProcessing_Init(&app->processing,cfg->minimum_scan_angle_mdeg,
          cfg->maximum_scan_angle_mdeg,cfg->scan_step_mdeg,
          cfg->average_window,cfg->background_threshold_mm,&cfg->detector,
          cfg->match_angle_mdeg,cfg->match_range_mm,cfg->lost_timeout_ms,
          cfg->max_misses)||
       !TargetTracker_SetLimit(&app->processing.tracker,operating_limit)||
       !TargetSelector_InitWithLimit(&app->selector,cfg->adc_max,
                                     cfg->adc_hysteresis,operating_limit))return false;
    ac.move=aim_cb;ac.context=app;ac.max_target_age_ms=cfg->aim_max_age_ms;
    ac.minimum_angle_mdeg=cfg->minimum_scan_angle_mdeg;
    ac.maximum_angle_mdeg=cfg->maximum_scan_angle_mdeg;
    if(!AimController_Init(&app->aim,&ac))return false;
    app->renderer.min_angle_mdeg=cfg->minimum_scan_angle_mdeg;
    app->renderer.max_angle_mdeg=cfg->maximum_scan_angle_mdeg;
    app->renderer.maximum_distance_mm=cfg->maximum_display_distance_mm;
    app->renderer.visible_target_limit=operating_limit;
    app->renderer.center_x=64u;app->renderer.center_y=62u;app->renderer.radius=58u;
    if(cfg->hw.lcd_write) {
        lc.write_line=lcd_cb;lc.context=app;lc.columns=16u;lc.rows=2u;
        if(!LcdStatus_Init(&app->lcd,&lc))return false;
    }
    launcher.indicator=led_cb;launcher.context=app;
    launcher.active_ms=cfg->simulation_ms;launcher.debounce_ms=cfg->button_debounce_ms;
    if(!Launcher_Init(&app->launcher,&launcher))return false;
    app->last_oled_ms=now;app->last_lcd_ms=now;
    RadarProcessing_RequestCalibration(&app->processing);
    if(!RadarScan_Start(&app->scan,now))return false;
    app->initialized=1u;
    return true;
}
void App_Update(App *app,uint32_t now){
    RadarSample sample;
    RadarTarget selected;
    uint16_t raw;
    uint8_t id;
    bool valid=false,aimed=false,moving;
    int32_t aim_angle=0;
    uint32_t dist=0;
    const char *label="NO TARGET";
    if(!app||!app->initialized)return;
    if(app->config.hw.echo_update)
        app->config.hw.echo_update(app->config.hw.context,now);
    RadarScan_Update(&app->scan,now);
    if(RadarScan_TakeSample(&app->scan,&sample)) {
        if(RadarProcessing_PushSample(&app->processing,&sample))
            RadarProcessing_ProcessCompleted(&app->processing,now);
    }
    if(app->config.hw.adc_read(app->config.hw.context,&raw))
        (void)TargetSelector_Update(&app->selector,raw);
    id=TargetSelector_GetId(&app->selector);
    if(app->aim.selected_id!=id)AimController_Select(&app->aim,id);
    AimController_Update(&app->aim,&app->processing.tracker,now);
    valid=TargetTracker_Get(&app->processing.tracker,id,&selected) &&
          selected.state!=TARGET_LOST &&
          (uint32_t)(now-selected.last_seen_ms)<=app->config.aim_max_age_ms;
    if(valid) {
        dist=selected.range_mm;
        label="TRACKING";
        if(app->config.hw.aim_position(app->config.hw.context,&aim_angle,&moving)){
            int64_t diff=(int64_t)aim_angle-selected.angle_mdeg;
            if(diff<0)diff=-diff;
            aimed=!moving && diff<=app->config.aim_tolerance_mdeg;
        }
    }
    if(!valid)label="LOST/NO TARGET";
    Launcher_Update(&app->launcher,now,valid,aimed);
    if(app->config.hw.oled_flush &&
       (uint32_t)(now-app->last_oled_ms)>=app->config.oled_period_ms){
        app->last_oled_ms=now;
        if(RadarRenderer_Draw(&app->renderer,&app->processing.display_frame,
             &app->processing.tracker,id,app->scan.angle_mdeg))
            (void)app->config.hw.oled_flush(app->config.hw.context);
    }
    if(app->config.hw.lcd_write &&
       (uint32_t)(now-app->last_lcd_ms)>=app->config.lcd_period_ms) {
        app->last_lcd_ms=now;
        (void)LcdStatus_Update(&app->lcd,id,valid?selected.angle_mdeg:0,
                              dist,label);
    }
}
void App_RequestFire(App *app,uint32_t now){
    if(app&&app->initialized)Launcher_Request(&app->launcher,now);
}
void App_RequestBackgroundCalibration(App *app){
    if(app&&app->initialized)RadarProcessing_RequestCalibration(&app->processing);
}
