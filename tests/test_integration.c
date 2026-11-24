#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "radar_frame.h"
#include "radar_processing.h"
#include "task_scheduler.h"
#include "lcd_status.h"
#include "launcher.h"
#include "radar_renderer.h"
#include "oled.h"
#include "app.h"

static unsigned pixel_count,draw_count,task_count,lcd_writes,led_changes;
static bool led_on;
static void task(void *ctx,uint32_t now){(void)ctx;(void)now;++task_count;}
static bool lcd_write(void *ctx,uint8_t row,const char *s){
    (void)ctx;(void)row;assert(s);++lcd_writes;return true;
}
static void led_write(void *ctx,bool on){(void)ctx;led_on=on;++led_changes;}
void OLED_Clear(void){pixel_count=0u;}
void OLED_DrawPixel(int16_t x,int16_t y,uint8_t c){
    (void)x;(void)y;(void)c;++pixel_count;
}
void OLED_DrawLine(int16_t x0,int16_t y0,int16_t x1,int16_t y1,uint8_t c){
    (void)x0;(void)y0;(void)x1;(void)y1;(void)c;++draw_count;
}
void OLED_DrawRect(int16_t x,int16_t y,int16_t w,int16_t h,uint8_t c){
    (void)x;(void)y;(void)w;(void)h;(void)c;++draw_count;
}
void OLED_FillRect(int16_t x,int16_t y,int16_t w,int16_t h,uint8_t c){
    (void)x;(void)y;(void)w;(void)h;(void)c;++draw_count;
}
void OLED_DrawChar(int16_t x,int16_t y,char c,uint8_t sz,uint8_t color){
    (void)x;(void)y;(void)c;(void)sz;(void)color;++draw_count;
}
static bool flush(void *ctx){(void)ctx;return true;}
static bool move(void *ctx,int32_t a){(void)ctx;(void)a;return true;}
static uint32_t mock_range=1000u;
static bool echo_pending;
static bool echo_start(void *ctx,uint32_t now){(void)ctx;(void)now;echo_pending=true;return true;}
static bool echo_take(void *ctx,uint32_t *dist,bool *valid){
    (void)ctx;if(!echo_pending)return false;
    echo_pending=false;*dist=mock_range;*valid=true;return true;
}
static bool adc_read(void *ctx,uint16_t *raw){(void)ctx;*raw=0u;return true;}
static bool aim_move(void *ctx,int32_t a){(void)ctx;(void)a;return true;}
static bool aim_pos(void *ctx,int32_t *a,bool *m){(void)ctx;*a=0;*m=false;return true;}
static void test_frames(void){
    RadarFrame f;RadarSample s={0,1000u,0u,true};
    assert(RadarFrame_Init(&f,0,120000,50000));
    assert(f.bins==4u);
    assert(!RadarFrame_Add(&f,&s));
    s.angle_mdeg=50000;assert(!RadarFrame_Add(&f,&s));
    s.angle_mdeg=100000;assert(!RadarFrame_Add(&f,&s));
    s.angle_mdeg=120000;assert(RadarFrame_Add(&f,&s));
    assert(RadarFrame_TakeCompleted(&f));
    assert(!RadarFrame_TakeCompleted(&f));
    assert(RadarFrame_Get(&f,3u,&s)&&s.angle_mdeg==120000);
}
static void test_processing(void){
    RadarProcessing p;
    TargetDetectorConfig dc={150u,40000,2u};
    RadarSample sample={0,1000u,0u,true};
    const int32_t first[]={0,30000,60000,90000,120000};
    const int32_t second[]={90000,60000,30000,0};
    assert(RadarProcessing_Init(&p,0,120000,30000,3,100u,&dc,40000,500u,1000u,2u));
    RadarProcessing_RequestCalibration(&p);
    for(unsigned i=0;i<5;i++){
        sample.angle_mdeg=first[i];sample.timestamp_ms=i*10u;
        if(RadarProcessing_PushSample(&p,&sample))
            RadarProcessing_ProcessCompleted(&p,50u);
    }
    assert(p.background_ready&&p.display_frame.frame_number==1u);
    for(unsigned i=0;i<4;i++){
        sample.angle_mdeg=second[i];sample.distance_mm=
            (second[i]==60000||second[i]==90000)?500u:1000u;
        sample.timestamp_ms=100u+i*10u;
        if(RadarProcessing_PushSample(&p,&sample))
            RadarProcessing_ProcessCompleted(&p,140u);
    }
    assert(p.candidate_count==1u);
    assert(p.display_frame.frame_number==2u);
    RadarTarget target;
    assert(TargetTracker_Get(&p.tracker,1u,&target));
    assert(target.state==TARGET_NEW);
    RadarRendererConfig renderer={0,120000,2000u,64u,62u,58u};
    assert(RadarRenderer_Draw(&renderer,&p.display_frame,&p.tracker,1u,30000));
    assert(draw_count>0u);
}
static void test_tasks_outputs(void){
    TaskScheduler scheduler;LcdStatus lcd;Launcher l;
    LcdStatusConfig lc={lcd_write,NULL,16u,2u};
    LauncherConfig led={led_write,NULL,40u,10u};
    TaskScheduler_Init(&scheduler);
    assert(TaskScheduler_Add(&scheduler,task,NULL,100u,0u));
    TaskScheduler_Run(&scheduler,0u);TaskScheduler_Run(&scheduler,50u);
    TaskScheduler_Run(&scheduler,250u);
    assert(task_count==2u);
    assert(LcdStatus_Init(&lcd,&lc));
    assert(LcdStatus_Update(&lcd,1u,45000,850u,"TRACKING"));
    assert(lcd_writes==2u);
    assert(LcdStatus_Update(&lcd,1u,45000,850u,"TRACKING"));
    assert(lcd_writes==2u);
    assert(Launcher_Init(&l,&led));
    Launcher_Request(&l,100u);
    Launcher_Update(&l,100u,true,false);
    assert(!Launcher_IsActive(&l));
    Launcher_Request(&l,120u);
    Launcher_Update(&l,120u,true,true);
    assert(Launcher_IsActive(&l)&&led_on);
    Launcher_Update(&l,170u,false,false);
    assert(!Launcher_IsActive(&l)&&!led_on);
}
static void test_app(void){
    static App app;
    AppConfig cfg={0};
    cfg.hw.servo_set_angle=move;
    cfg.hw.echo_start=echo_start;
    cfg.hw.echo_take=echo_take;
    cfg.hw.adc_read=adc_read;
    cfg.hw.aim_move=aim_move;
    cfg.hw.aim_position=aim_pos;
    cfg.hw.oled_flush=flush;
    cfg.hw.lcd_write=lcd_write;
    cfg.hw.led_write=led_write;
    cfg.minimum_scan_angle_mdeg=30000;cfg.maximum_scan_angle_mdeg=90000;
    cfg.scan_step_mdeg=30000;cfg.settle_ms=10;cfg.echo_timeout_ms=30;
    cfg.average_window=2u;cfg.background_threshold_mm=100u;
    cfg.detector.max_range_difference_mm=200u;
    cfg.detector.max_angle_gap_mdeg=40000;
    cfg.detector.min_cluster_samples=1u;
    cfg.match_angle_mdeg=40000;cfg.match_range_mm=500u;
    cfg.lost_timeout_ms=1000u;cfg.max_misses=2u;
    cfg.adc_max=4095u;cfg.adc_hysteresis=20u;
    cfg.maximum_display_distance_mm=2000u;
    cfg.oled_period_ms=100u;cfg.lcd_period_ms=100u;
    cfg.aim_max_age_ms=200u;cfg.aim_tolerance_mdeg=2000;
    cfg.simulation_ms=40u;cfg.button_debounce_ms=10u;
    assert(App_Init(&app,&cfg,0u));
    assert(app.config.operating_target_limit==3u);
    assert(app.selector.selectable_targets==3u);
    assert(app.processing.tracker.active_limit==3u);
    assert(app.renderer.visible_target_limit==3u);
    for(uint32_t ms=0u;ms<300u;ms+=5u)App_Update(&app,ms);
    assert(app.processing.background_ready);
    assert(app.processing.display_frame.frame_number>=1u);
    mock_range=400u;
    for(uint32_t ms=300u;ms<600u;ms+=5u)App_Update(&app,ms);
    assert(app.processing.display_frame.frame_number>=2u);
    cfg.operating_target_limit=5u;
    assert(App_Init(&app,&cfg,0u));
    assert(app.selector.selectable_targets==5u);
    assert(app.processing.tracker.active_limit==5u);
    assert(app.renderer.visible_target_limit==5u);
    cfg.operating_target_limit=6u;
    assert(!App_Init(&app,&cfg,0u));
}
int main(void){
    test_frames();test_processing();test_tasks_outputs();test_app();
    puts("PASS: frame, per-bin processing, tracker, renderer, scheduler, LCD, LED, App");
    return 0;
}
