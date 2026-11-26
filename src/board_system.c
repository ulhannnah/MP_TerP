#include "board_system.h"
#include "clocks_and_modes.h"
#include "oled.h"
#include "aim_geometry.h"
#include "relay_policy.h"
#include "final_io_pins.h"
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
    int32_t relative;
    if(!MP_TERP_AimToStepper(angle,b->aiming_scan_center_mdeg,&relative))
        return false;
    return StepperController_SetTargetAngle(&b->stepper,relative)==STEPPER_OK;
}
static bool aim_stop(void *ctx){
    BoardSystem *b=(BoardSystem *)ctx;
    /* STOP cuts the timer-driven steps; release the ULN2003 coils as well. */
    if(StepperController_Stop(&b->stepper)!=STEPPER_OK)return false;
    return StepperController_Release(&b->stepper)==STEPPER_OK;
}
static bool aim_position(void *ctx,int32_t *angle,bool *moving){
    BoardSystem *b=(BoardSystem *)ctx;
    int32_t relative=0;
    if(angle==NULL ||
       StepperController_GetPosition(&b->stepper,&relative,NULL,moving)!=STEPPER_OK)
        return false;
    /* Stepper position is relative; App compares absolute scan angles. */
    return MP_TERP_StepperToAim(relative,b->aiming_scan_center_mdeg,angle);
}
static bool oled_flush(void *ctx){(void)ctx;return OLED_Update()==0;}
static bool lcd_write(void *ctx,uint8_t row,const char *line){
    BoardSystem *b=(BoardSystem *)ctx;
    return b->config.lcd_write?b->config.lcd_write(b->config.user,row,line):false;
}
static void led_write(void *ctx,bool on){
    BoardSystem *b=(BoardSystem *)ctx;
    if(b->config.use_gpio_led) GpioOutput_Write(&b->led,on);
    else if(b->config.led_write)b->config.led_write(b->config.user,on);
}
bool BoardSystem_Init(BoardSystem *b,const BoardSystemConfig *cfg){
    AppConfig app;
    StepperControllerConfig stepper;
    if(!b||!cfg||(!cfg->led_write&&!cfg->use_gpio_led)||!cfg->stepper_delay||
       cfg->timer.tick_channel==TIMER_CHANNEL_NONE||
       cfg->servo.ftm==cfg->ultrasonic.ftm||
       cfg->stepper.timer_channel==cfg->ultrasonic.trigger_timer_channel||
       cfg->stepper.timer_channel==cfg->timer.tick_channel||
       cfg->ultrasonic.trigger_timer_channel==cfg->timer.tick_channel ||
       cfg->aiming_scan_center_mdeg < cfg->application.minimum_scan_angle_mdeg ||
       cfg->aiming_scan_center_mdeg > cfg->application.maximum_scan_angle_mdeg ||
       (cfg->lcd_write && !cfg->lcd_init) ||
       (cfg->use_home_button && (!cfg->use_gpio_button ||
          cfg->home_button_gpio.port!=cfg->button_gpio.port ||
          cfg->home_button_gpio.pin==cfg->button_gpio.pin)) ||
       (cfg->use_relay_button &&
          (!cfg->use_gpio_button || !cfg->use_relay_output ||
           cfg->relay_button_gpio.port!=cfg->button_gpio.port ||
           cfg->relay_button_gpio.pin==cfg->button_gpio.pin ||
           !cfg->relay_button_gpio.active_low ||
           !cfg->button_gpio.active_low ||
           cfg->external_button_debounce_ms==0u)) ||
       (cfg->use_relay_output &&
          (cfg->relay_pulse_ms==0u || !cfg->relay_output_gpio.active_high)) ||
       (cfg->use_external_adc_pin &&
          (!cfg->external_adc_port || cfg->external_adc_pin>=32u)) ||
       /* SWD_CLK=PTC4 and JTAG_TDI=PTC5. NEVER remux either as GPIO,
        * regardless of which external input role might claim it. */
       (cfg->use_gpio_button && cfg->button_gpio.port==PORTC &&
         MP_TERP_PortCDebugPinReserved(cfg->button_gpio.pin)) ||
       (cfg->use_home_button && cfg->home_button_gpio.port==PORTC &&
         MP_TERP_PortCDebugPinReserved(cfg->home_button_gpio.pin)) ||
       (cfg->use_relay_button && cfg->relay_button_gpio.port==PORTC &&
         MP_TERP_PortCDebugPinReserved(cfg->relay_button_gpio.pin)))
       return false;
    b->initialized=0u;b->adc_pending=0u;b->config=*cfg;
    b->relay_active=0u;b->relay_armed=0u;
    b->previous_fire_state=(uint8_t)APP_FIRE_IDLE;
    b->relay_started_ms=0u;b->relay_pulse_count=0u;
    b->external_aim_press_count=0u;b->external_relay_press_count=0u;
    b->aiming_scan_center_mdeg=cfg->aiming_scan_center_mdeg;
    SOSC_init_8MHz();
    SPLL_init_160MHz();
    NormalRUNmode_80MHz();
    if(TimerManager_Init(&cfg->timer)!=TIMER_OK)return false;
    if(cfg->use_external_adc_pin) {
        /* PTB0 must be routed as analog MUX0, never as digital output. */
        PCC->PCCn[cfg->external_adc_port_pcc_index]|=PCC_PCCn_CGC_MASK;
        cfg->external_adc_port->PCR[cfg->external_adc_pin]=PORT_PCR_MUX(0);
    }
    if(!ADCChannel_Init(&b->adc,&cfg->adc))return false;
    if(cfg->use_gpio_led && !GpioOutput_Init(&b->led,&cfg->led_gpio))return false;
    /* Relay output must be LOW well before any input IRQ is enabled. */
    if(cfg->use_relay_output &&
       !GpioOutput_Init(&b->relay,&cfg->relay_output_gpio))return false;
    if(cfg->use_gpio_button && !GpioButton_Init(&b->button,&cfg->button_gpio))return false;
    if(cfg->use_home_button &&
       !GpioButton_Init(&b->home_button,&cfg->home_button_gpio))return false;
    if(cfg->use_relay_button &&
       !GpioButton_Init(&b->relay_button,&cfg->relay_button_gpio))return false;
    if(StepMotor_Init(&b->step_motor,cfg->stepper_pins,cfg->stepper_delay)!=0)return false;
    StepMotor_SetMode(&b->step_motor,cfg->stepper_mode);
    stepper=cfg->stepper;stepper.motor=&b->step_motor;
    if(StepperController_Init(&b->stepper,&stepper)!=STEPPER_OK)return false;
    if(cfg->assume_stepper_center_on_boot &&
       StepperController_SetHome(&b->stepper,0)!=STEPPER_OK)return false;
    /* A startup zero is an OPEN-LOOP declaration: the operator must align
     * the shaft to the scanner's physical center before applying power. */
    if(Servo_Init(&b->servo,&cfg->servo)!=SERVO_OK)return false;
    if(cfg->servo_calibration_count!=0u &&
       Servo_SetCalibration(&b->servo,cfg->servo_calibration,
                            cfg->servo_calibration_count)!=SERVO_OK)return false;
    if(!UltrasonicPort_Init(&b->ultrasonic,&cfg->ultrasonic))return false;
    if(cfg->custom_oled_port && !OLED_PortConfigure(&cfg->oled_port))return false;
    if(OLED_Init()!=0)return false;
    if(cfg->lcd_write && !cfg->lcd_init(cfg->user))return false;
    app=cfg->application;
    app.hw.context=b;
    app.hw.servo_set_angle=servo_move;
    app.hw.echo_start=echo_start;
    app.hw.echo_take=echo_take;
    app.hw.echo_update=echo_update;
    app.hw.adc_read=adc_read;
    app.hw.aim_move=aim_move;
    app.hw.aim_stop=aim_stop;
    app.hw.aim_position=aim_position;
    app.hw.oled_flush=oled_flush;
    app.hw.lcd_write=cfg->lcd_write?lcd_write:NULL;
    app.hw.led_write=led_write;
    if(!App_Init(&b->app,&app,TimerManager_GetTickMs()))return false;
    b->initialized=1u;
    return true;
}
void BoardSystem_Update(BoardSystem *b){
    uint32_t now;
    bool moving=true;
    if(!b||!b->initialized)return;
    now=TimerManager_GetTickMs();

    /* Pulse must end even if the relay button remains held LOW. */
    if(MP_RelayPulseExpired(b->relay_active!=0u,now,
                            b->relay_started_ms,b->config.relay_pulse_ms)) {
        GpioOutput_Write(&b->relay,false);
        b->relay_active=0u;
    }

    /* The ISR sets flags only. All debouncing, App/Step control, and GPIO
     * relay writes run here in foreground. No motor logic runs in the ISR. */
    if(b->config.use_home_button && GpioButton_TakePress(&b->home_button) &&
       !StepperController_IsHomed(&b->stepper))
        (void)BoardSystem_ConfirmStepperCenter(b);
    if(b->config.use_gpio_button &&
       GpioButton_TakePressDebounced(&b->button,now,
                                    b->config.external_button_debounce_ms)) {
        ++b->external_aim_press_count;
        if(!b->relay_active) {
            b->relay_armed=0u;
            App_RequestFire(&b->app,now);
        }
    }

    App_Update(&b->app,now);
    if(MP_RelayNewAimCompleted(b->previous_fire_state,
                               (uint8_t)b->app.fire_state,
                               (uint8_t)APP_FIRE_INDICATING))
        b->relay_armed=1u;
    else if(b->app.fire_state==APP_FIRE_TIMED_OUT) {
        b->relay_armed=0u;
        if(b->relay_active) {
            GpioOutput_Write(&b->relay,false);
            b->relay_active=0u;
        }
    }
    b->previous_fire_state=(uint8_t)b->app.fire_state;

    if(b->config.use_relay_button &&
       GpioButton_TakePressDebounced(&b->relay_button,now,
                                    b->config.external_button_debounce_ms)) {
        ++b->external_relay_press_count;
        /* Independent relay button: one bounded pulse only after a completed
         * Stepper software aim, never while moving or before software home. */
        if(StepperController_GetPosition(&b->stepper,NULL,NULL,
                                         &moving)==STEPPER_OK &&
           MP_RelayCanPulse(b->relay_armed!=0u,b->relay_active!=0u,
                            StepperController_IsHomed(&b->stepper),moving)) {
            GpioOutput_Write(&b->relay,true);
            b->relay_active=1u;
            b->relay_armed=0u;
            b->relay_started_ms=now;
            ++b->relay_pulse_count;
        }
    }
    /* App may queue the latest status text. Emit at most one GPIO edge
     * step, returning immediately to the scan/tracker main loop. */
    if(b->config.lcd_service)
        b->config.lcd_service(b->config.user,TimerManager_GetTickMs());
}
void BoardSystem_OnEchoCaptureIrq(BoardSystem *b){
    if(b&&b->initialized)UltrasonicPort_OnCaptureIrq(&b->ultrasonic);
}
void BoardSystem_OnFireButton(BoardSystem *b){
    if(b&&b->initialized)App_RequestFire(&b->app,TimerManager_GetTickMs());
}

void BoardSystem_OnFireButtonPortIrq(BoardSystem *b){
    if(!b||!b->initialized)return;
    if(b->config.use_gpio_button)GpioButton_OnPortIrq(&b->button);
    if(b->config.use_home_button)GpioButton_OnPortIrq(&b->home_button);
    if(b->config.use_relay_button)GpioButton_OnPortIrq(&b->relay_button);
}

bool BoardSystem_ConfirmStepperCenter(BoardSystem *b)
{
    if(!b || !b->initialized || StepperController_IsHomed(&b->stepper))
        return false;
    return StepperController_SetHome(&b->stepper,0)==STEPPER_OK;
}
