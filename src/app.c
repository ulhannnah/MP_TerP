#include "app.h"
#include "oled.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

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
       cfg->aim_tolerance_mdeg>360000u||cfg->maximum_display_distance_mm==0u||
       cfg->uncoupled_diagnostic>1u||cfg->diagnostic_manual_aim>1u||
       (cfg->fire_snapshot_mode!=0u &&
        (cfg->fire_aim_timeout_ms==0u || !cfg->hw.aim_stop)))
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
    app->fire_state=APP_FIRE_IDLE;
    app->fire_snapshot_id=0u;
    app->fire_snapshot_angle_mdeg=0;
    app->fire_requested_at_ms=0u;
    app->diagnostic_samples=0u;
    app->diagnostic_range_mm=0u;
    app->diagnostic_valid=0u;
    app->last_adc_raw=0u;
    app->diagnostic_selected_angle_mdeg=cfg->minimum_scan_angle_mdeg;
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
    if(app->config.uncoupled_diagnostic) {
        /* Sensor and Servo are physically separate: sweep the Servo and
         * read FIXED-direction Echo for bring-up, but DO NOT associate
         * these distances with the commanded Servo angle. */
        if(RadarScan_TakeSample(&app->scan,&sample)) {
            ++app->diagnostic_samples;
            app->diagnostic_valid=sample.valid?1u:0u;
            app->diagnostic_range_mm=sample.valid?sample.distance_mm:0u;
        }
        /* External potentiometer selects a MANUAL azimuth, not a target ID.
         * No angular information is inferred from this fixed-direction Echo. */
        if(app->config.diagnostic_manual_aim &&
           app->config.hw.adc_read(app->config.hw.context,&raw)) {
            const uint32_t adc=(raw>app->config.adc_max)?
                                app->config.adc_max:raw;
            const uint32_t span=(uint32_t)(app->config.maximum_scan_angle_mdeg-
                                           app->config.minimum_scan_angle_mdeg);
            app->last_adc_raw=(uint16_t)adc;
            app->diagnostic_selected_angle_mdeg=
                app->config.minimum_scan_angle_mdeg+
                (int32_t)(((uint64_t)span*adc+app->config.adc_max/2u)/
                          app->config.adc_max);
        }
        if(app->config.diagnostic_manual_aim &&
           app->fire_state==APP_FIRE_AIMING) {
            if((uint32_t)(now-app->fire_requested_at_ms)>=
               app->config.fire_aim_timeout_ms) {
                (void)app->config.hw.aim_stop(app->config.hw.context);
                app->fire_state=APP_FIRE_TIMED_OUT;
            } else {
                int32_t angle=0;
                bool in_motion=true;
                if(app->config.hw.aim_position(app->config.hw.context,
                                               &angle,&in_motion)) {
                    int64_t error=(int64_t)angle-app->fire_snapshot_angle_mdeg;
                    if(error<0)error=-error;
                    if(!in_motion && error<=app->config.aim_tolerance_mdeg)
                        app->fire_state=APP_FIRE_INDICATING;
                }
            }
        }
        /* In manual diagnostic APP_FIRE_INDICATING means software step count
         * arrived, NOT that a physical encoder measured the position.
         * The external relay has its own button and is never auto-fired. */
        if(app->config.diagnostic_manual_aim &&
           app->fire_state==APP_FIRE_INDICATING &&
           (uint32_t)(now-app->fire_requested_at_ms)>
             app->config.fire_aim_timeout_ms)
            app->fire_state=APP_FIRE_IDLE;
        /* Never fill 2D frames or create target IDs. User ADC angle is
         * displayed as MANUAL and cannot be mistaken for measured bearing. */
        if(app->config.hw.oled_flush &&
           (uint32_t)(now-app->last_oled_ms)>=app->config.oled_period_ms) {
            app->last_oled_ms=now;
            OLED_Clear();
            OLED_DrawString(0,0,"FIXED SENSOR",1u,OLED_WHITE);
            OLED_DrawString(0,16,"SERVO SWEEP ONLY",1u,OLED_WHITE);
            OLED_DrawString(0,32,"NO 2D TARGETS",1u,OLED_WHITE);
            if(app->config.diagnostic_manual_aim) {
                char manual[32];
                (void)snprintf(manual,sizeof(manual),"MANUAL %ld DEG",
                     (long)(app->diagnostic_selected_angle_mdeg/1000));
                OLED_DrawString(0,48,manual,1u,OLED_WHITE);
            }
            (void)app->config.hw.oled_flush(app->config.hw.context);
        }
        if(app->config.hw.lcd_write &&
           (uint32_t)(now-app->last_lcd_ms)>=app->config.lcd_period_ms) {
            char text[32]; /* snprintf buffer; LCD driver trims to 16 columns */
            app->last_lcd_ms=now;
            if(app->diagnostic_valid) {
                (void)snprintf(text,sizeof(text),"ECHO:%lumm",
                               (unsigned long)app->diagnostic_range_mm);
            } else {
                (void)snprintf(text,sizeof(text),"ECHO:INVALID");
            }
            (void)app->config.hw.lcd_write(app->config.hw.context,0u,text);
            if(app->config.diagnostic_manual_aim) {
                (void)snprintf(text,sizeof(text),"MANUAL:%ld DEG",
                     (long)(app->diagnostic_selected_angle_mdeg/1000));
                (void)app->config.hw.lcd_write(app->config.hw.context,1u,text);
            } else {
                (void)app->config.hw.lcd_write(app->config.hw.context,1u,
                                               "FIXED 1D / NO ID");
            }
        }
        return;
    }
    if(RadarScan_TakeSample(&app->scan,&sample)) {
        if(RadarProcessing_PushSample(&app->processing,&sample))
            RadarProcessing_ProcessCompleted(&app->processing,now);
    }
    if(app->config.hw.adc_read(app->config.hw.context,&raw)) {
        app->last_adc_raw=raw;
        (void)TargetSelector_Update(&app->selector,raw);
    }
    id=TargetSelector_GetId(&app->selector);
    if(app->aim.selected_id!=id)AimController_Select(&app->aim,id);
    /* Snapshot mode never silently retargets while moving to a captured
     * button-time angle. Legacy mode preserves continuous tracking. */
    if(app->config.fire_snapshot_mode==0u)
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
    if(app->config.fire_snapshot_mode==0u) {
        Launcher_Update(&app->launcher,now,valid,aimed);
    } else {
        if(app->fire_state==APP_FIRE_AIMING) {
            if((uint32_t)(now-app->fire_requested_at_ms) >=
               app->config.fire_aim_timeout_ms) {
                /* Stop the timer-driven Stepper and release its coils. This
                 * does not certify a physical position (no encoder). */
                (void)app->config.hw.aim_stop(app->config.hw.context);
                app->fire_state=APP_FIRE_TIMED_OUT;
            } else {
                int32_t actual=0;
                bool still_moving=true;
                if(app->config.hw.aim_position(app->config.hw.context,
                                                &actual,&still_moving)) {
                    int64_t error=(int64_t)actual-app->fire_snapshot_angle_mdeg;
                    if(error<0)error=-error;
                    if(!still_moving && error<=app->config.aim_tolerance_mdeg) {
                        Launcher_Request(&app->launcher,now);
                        Launcher_Update(&app->launcher,now,true,true);
                        app->fire_state=APP_FIRE_INDICATING;
                    }
                }
            }
        }
        /* Keep the LED pulse's expiry active independently of target changes. */
        Launcher_Update(&app->launcher,now,false,false);
        if(app->fire_state==APP_FIRE_INDICATING &&
           !Launcher_IsActive(&app->launcher))
            app->fire_state=APP_FIRE_IDLE;
    }
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
    RadarTarget target;
    uint8_t id;
    if(!app||!app->initialized)return;
    if(app->config.uncoupled_diagnostic) {
        /* Manual azimuth demonstration. It does not use sonar to infer a
         * target angle. The Stepper must already have an established zero. */
        if(!app->config.diagnostic_manual_aim ||
           (app->fire_state!=APP_FIRE_IDLE &&
            app->fire_state!=APP_FIRE_INDICATING &&
            app->fire_state!=APP_FIRE_TIMED_OUT))return;
        const int32_t angle=app->diagnostic_selected_angle_mdeg;
        if(!app->config.hw.aim_move(app->config.hw.context,angle))return;
        app->fire_snapshot_angle_mdeg=angle;
        app->fire_snapshot_id=0u; /* no detected target */
        app->fire_requested_at_ms=now;
        app->fire_state=APP_FIRE_AIMING;
        return;
    }
    if(app->config.fire_snapshot_mode==0u) {
        Launcher_Request(&app->launcher,now);
        return;
    }
    if(app->fire_state==APP_FIRE_TIMED_OUT)app->fire_state=APP_FIRE_IDLE;
    if(app->fire_state!=APP_FIRE_IDLE || Launcher_IsActive(&app->launcher))return;
    id=TargetSelector_GetId(&app->selector);
    if(!TargetTracker_Get(&app->processing.tracker,id,&target) ||
       target.state==TARGET_LOST || target.state==TARGET_EMPTY ||
       (uint32_t)(now-target.last_seen_ms)>app->config.aim_max_age_ms ||
       target.angle_mdeg<app->config.minimum_scan_angle_mdeg ||
       target.angle_mdeg>app->config.maximum_scan_angle_mdeg)
        return;
    /* Only a valid, fresh target may initiate motion; capture this exact
     * command value before any later tracker updates. */
    if(!app->config.hw.aim_move(app->config.hw.context,target.angle_mdeg))
        return;
    app->fire_snapshot_id=id;
    app->fire_snapshot_angle_mdeg=target.angle_mdeg;
    app->fire_requested_at_ms=now;
    app->fire_state=APP_FIRE_AIMING;
}
void App_RequestBackgroundCalibration(App *app){
    if(app&&app->initialized)RadarProcessing_RequestCalibration(&app->processing);
}
