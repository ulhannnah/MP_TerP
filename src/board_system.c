#include "board_system.h"
#include "clocks_and_modes.h"
#include "oled.h"
#include <stddef.h>
static bool servo_move(void *ctx,int32_t angle){
    BoardSystem *b=(BoardSystem *)ctx;
    return Servo_SetAngle(&b->servo,angle)==SERVO_OK;
}
static bool echo_start(void *ctx,uint32_t now){
    BoardSystem *b=(BoardSystem *)ctx;
    return UltrasonicPort_Start(&b->ultrasonic,now);
}
static bool echo_take(void *ctx,uint32_t *mm,bool *valid){
    BoardSystem *b=(BoardSystem *)ctx;
    return UltrasonicPort_Take(&b->ultrasonic,mm,valid);
}
static void echo_update(void *ctx,uint32_t now){
    BoardSystem *b=(BoardSystem *)ctx;
    UltrasonicPort_Update(&b->ultrasonic,now);
}
static bool adc_read(void *ctx,uint16_t *raw){
    BoardSystem *b=(BoardSystem *)ctx;
    if(!b->adc_pending) {
        b->adc_pending=ADCChannel_Start(&b->adc)?1u:0u;
        return false;
    }
    if(!ADCChannel_Poll(&b->adc,raw))return false;
    b->adc_pending=0u;
    return true;
}
static bool aim_move(void *ctx,int32_t angle){
    BoardSystem *b=(BoardSystem *)ctx;
    return StepperController_SetTargetAngle(&b->stepper,angle)==STEPPER_OK;
}
static bool aim_position(void *ctx,int32_t *angle,bool *moving){
    BoardSystem *b=(BoardSystem *)ctx;
    return StepperController_GetPosition(&b->stepper,angle,NULL,moving)==STEPPER_OK;
}
static bool oled_flush(void *ctx){(void)ctx;return OLED_Update()==0;}
static bool lcd_write(void *ctx,uint8_t row,const char *line){
    BoardSystem *b=(BoardSystem *)ctx;
    return b->config.lcd_write?b->config.lcd_write(b->config.user,row,line):false;
}
static void led_write(void *ctx,bool on){
    BoardSystem *b=(BoardSystem *)ctx;
    b->config.led_write(b->config.user,on);
}
bool BoardSystem_Init(BoardSystem *b,const BoardSystemConfig *cfg){
    AppConfig app;
    StepperControllerConfig stepper;
    if(!b||!cfg||!cfg->led_write||!cfg->stepper_delay||
       cfg->timer.tick_channel==TIMER_CHANNEL_NONE||
       cfg->servo.ftm==cfg->ultrasonic.ftm||
       cfg->stepper.timer_channel==cfg->ultrasonic.trigger_timer_channel||
       cfg->stepper.timer_channel==cfg->timer.tick_channel||
       cfg->ultrasonic.trigger_timer_channel==cfg->timer.tick_channel)
       return false;
    b->initialized=0u;b->adc_pending=0u;b->config=*cfg;
    SOSC_init_8MHz();
    SPLL_init_160MHz();
    NormalRUNmode_80MHz();
    if(TimerManager_Init(&cfg->timer)!=TIMER_OK)return false;
    if(!ADCChannel_Init(&b->adc,&cfg->adc))return false;
    if(StepMotor_Init(&b->step_motor,cfg->stepper_pins,cfg->stepper_delay)!=0)return false;
    StepMotor_SetMode(&b->step_motor,cfg->stepper_mode);
    stepper=cfg->stepper;stepper.motor=&b->step_motor;
    if(StepperController_Init(&b->stepper,&stepper)!=STEPPER_OK)return false;
    /* Real homing is not automatic. Board commissioning must confirm
     * the reference, then call StepperController_SetHome() explicitly. */
    if(Servo_Init(&b->servo,&cfg->servo)!=SERVO_OK)return false;
    if(!UltrasonicPort_Init(&b->ultrasonic,&cfg->ultrasonic))return false;
    if(cfg->custom_oled_port && !OLED_PortConfigure(&cfg->oled_port))return false;
    if(OLED_Init()!=0)return false;
    app=cfg->application;
    app.hw.context=b;
    app.hw.servo_set_angle=servo_move;
    app.hw.echo_start=echo_start;
    app.hw.echo_take=echo_take;
    app.hw.echo_update=echo_update;
    app.hw.adc_read=adc_read;
    app.hw.aim_move=aim_move;
    app.hw.aim_position=aim_position;
    app.hw.oled_flush=oled_flush;
    app.hw.lcd_write=cfg->lcd_write?lcd_write:NULL;
    app.hw.led_write=led_write;
    if(!App_Init(&b->app,&app,TimerManager_GetTickMs()))return false;
    b->initialized=1u;
    return true;
}
void BoardSystem_Update(BoardSystem *b){
    if(!b||!b->initialized)return;
    App_Update(&b->app,TimerManager_GetTickMs());
}
void BoardSystem_OnEchoCaptureIrq(BoardSystem *b){
    if(b&&b->initialized)UltrasonicPort_OnCaptureIrq(&b->ultrasonic);
}
void BoardSystem_OnFireButton(BoardSystem *b){
    if(b&&b->initialized)App_RequestFire(&b->app,TimerManager_GetTickMs());
}
