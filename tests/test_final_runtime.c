#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "app_profile.h"
#include "oled.h"

/* Host-only integration: same final App and processing C, synthetic GPIO/
 * ultrasonic/stepper callbacks. NEVER used to claim physical radar accuracy. */
static int32_t servo_angle=90000, stepper_angle=90000, aim_command=0;
static uint32_t servo_commands, echo_count, aim_commands, stop_commands, led_on_count;
static bool echo_pending, targets_present, stepper_homed, stepper_moving, led_active;

void OLED_Clear(void){}
void OLED_DrawPixel(int16_t x,int16_t y,uint8_t c)
{(void)x;(void)y;(void)c;}
void OLED_DrawLine(int16_t a,int16_t b,int16_t c,int16_t d,uint8_t e)
{(void)a;(void)b;(void)c;(void)d;(void)e;}
void OLED_DrawRect(int16_t a,int16_t b,int16_t c,int16_t d,uint8_t e)
{(void)a;(void)b;(void)c;(void)d;(void)e;}
void OLED_FillRect(int16_t a,int16_t b,int16_t c,int16_t d,uint8_t e)
{(void)a;(void)b;(void)c;(void)d;(void)e;}
void OLED_DrawChar(int16_t a,int16_t b,char c,uint8_t d,uint8_t e)
{(void)a;(void)b;(void)c;(void)d;(void)e;}
static bool servo_set(void *ctx,int32_t angle)
{
    (void)ctx;servo_angle=angle;++servo_commands;
    return angle>=30000 && angle<=150000;
}
static bool echo_start(void *ctx,uint32_t ms)
{
    (void)ctx;(void)ms;echo_pending=true;return true;
}
static bool echo_take(void *ctx,uint32_t *mm,bool *valid)
{
    (void)ctx;
    if(!echo_pending)return false;
    echo_pending=false;
    *mm=3000u;
    if(targets_present){
        if(servo_angle>=60000 && servo_angle<=70000)*mm=1200u;
        if(servo_angle>=105000 && servo_angle<=115000)*mm=1900u;
    }
    *valid=true;
    ++echo_count;
    return true;
}
static bool adc_read(void *ctx,uint16_t *raw)
{(void)ctx;*raw=0u;return true;} /* selected ID 1 */
static bool aim_move(void *ctx,int32_t angle)
{
    (void)ctx;
    if(!stepper_homed)return false;
    aim_command=angle;stepper_moving=true;++aim_commands;return true;
}
static bool aim_position(void *ctx,int32_t *angle,bool *moving)
{
    (void)ctx;
    if(!stepper_homed)return false;
    *angle=stepper_angle;*moving=stepper_moving;return true;
}
static bool aim_stop(void *ctx)
{
    (void)ctx;++stop_commands;stepper_moving=false;return true;
}
static void led_write(void *ctx,bool on)
{
    (void)ctx;
    if(on && !led_active)++led_on_count;
    led_active=on;
}
int main(void)
{
    static App app;
    AppConfig cfg=MP_TERP_Radar120Profile();
    uint32_t ms, before;
    RadarTarget track;

    cfg.hw.servo_set_angle=servo_set;
    cfg.hw.echo_start=echo_start;
    cfg.hw.echo_take=echo_take;
    cfg.hw.adc_read=adc_read;
    cfg.hw.aim_move=aim_move;
    cfg.hw.aim_stop=aim_stop;
    cfg.hw.aim_position=aim_position;
    cfg.hw.led_write=led_write;
    assert(cfg.fire_snapshot_mode==1u);
    assert(App_Init(&app,&cfg,0u));
    assert(app.scan.angle_mdeg==30000);
    assert(servo_commands==1u && app.fire_state==APP_FIRE_IDLE);
    for(ms=0u;ms<6000u;ms++){
        App_Update(&app,ms);
        if(app.processing.background_ready)break;
    }
    assert(app.processing.background_ready);
    assert(app.processing.display_frame.frame_number>=1u);
    assert(echo_count>=25u && servo_commands>=25u);

    targets_present=true;
    for(;ms<13000u;ms++){
        App_Update(&app,ms);
        if(TargetTracker_Get(&app.processing.tracker,1u,&track) &&
           (track.state==TARGET_NEW||track.state==TARGET_TRACKING) &&
           app.processing.candidate_count>=1u)
            break;
    }
    assert(ms<13000u);
    assert(track.angle_mdeg>=60000 && track.angle_mdeg<=70000);
    assert(TargetSelector_GetId(&app.selector)==1u);

    /* SW2 before physical SW3 home confirmation must never move Stepper. */
    before=aim_commands;
    App_RequestFire(&app,ms);
    assert(app.fire_state==APP_FIRE_IDLE);
    assert(aim_commands==before && !led_active);
    stepper_homed=true; /* user confirms physical center with SW3 */
    App_RequestFire(&app,ms+1u);
    assert(app.fire_state==APP_FIRE_AIMING);
    assert(app.fire_snapshot_id==1u);
    assert(aim_commands==before+1u);
    assert(aim_command==app.fire_snapshot_angle_mdeg);

    App_Update(&app,ms+2u);
    assert(app.fire_state==APP_FIRE_AIMING && !led_active);
    stepper_angle=aim_command;
    stepper_moving=false;
    App_Update(&app,ms+3u);
    assert(app.fire_state==APP_FIRE_INDICATING && led_active);
    assert(led_on_count==1u);
    App_Update(&app,ms+cfg.simulation_ms+4u);
    assert(app.fire_state==APP_FIRE_IDLE && !led_active);
    assert(stop_commands==0u);
    puts("PASS: final auto scan, background, synthetic target ID, manual home guard, SW2 aim, LED");
    return 0;
}
