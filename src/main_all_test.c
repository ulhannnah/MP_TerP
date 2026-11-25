/*
 * Full-peripheral bring-up on REAL hardware, without false 2-D radar angles.
 *
 * Build with MP_TERP_ALL_TEST_MAIN (and not MP_TERP_STAGE_MAIN).
 * User must provide a VERIFIED MP_TERP_GetBoardConfig() plus LCD callback
 * initialized by MP_TERP_AllTestLCDInit() after the clock is running.
 *
 * All inputs/displays and both motors' driver interfaces are initialized:
 * - Servo holds its configured initial angle until debugger command.
 * - Ultrasonic measures fixed-direction distance every ~100ms.
 * - ADC potentiometer read continuously; OLED/LCD display raw diagnostics.
 * - Stepper holds still until explicit physical home and target commands.
 * - LED toggles as a heartbeat; button counts presses.
 *
 * NO automatic radar sweep, fake angle inference, or simulated launcher.
 */
#if defined(MP_TERP_ALL_TEST_MAIN)
#if defined(MP_TERP_STAGE_MAIN) || defined(MP_TERP_USE_EXAMPLE_MAIN)
#error "Select exactly one main mode"
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include "board_system.h"
#include "clocks_and_modes.h"
#include "oled.h"

extern bool MP_TERP_GetBoardConfig(BoardSystemConfig *cfg);
extern bool MP_TERP_AllTestLCDInit(void);

volatile uint32_t g_all_state;        /* 0 boot, 1 ready, 2 running, 0xDEAD failure */
volatile uint32_t g_all_error;
volatile uint32_t g_all_tick_ms;
volatile uint32_t g_all_echo_samples;
volatile uint32_t g_all_echo_valid;
volatile uint32_t g_all_echo_timeout;
volatile uint32_t g_all_range_mm;
volatile uint32_t g_all_adc_raw;
volatile uint32_t g_all_button_presses;
volatile uint32_t g_all_oled_updates;
volatile uint32_t g_all_lcd_updates;
volatile uint32_t g_all_heartbeat;
volatile int32_t g_all_servo_request_mdeg;
volatile int32_t g_all_servo_applied_mdeg;
volatile uint32_t g_all_stepper_home_confirm; /* write 0x484F4D45 only after positioning */
volatile int32_t g_all_stepper_home_angle_mdeg;
volatile int32_t g_all_stepper_request_mdeg;
volatile int32_t g_all_stepper_applied_mdeg;
volatile uint32_t g_all_stepper_homed;
volatile uint32_t g_all_stepper_moving;

static BoardSystemConfig s_cfg;
static Servo s_servo;
static UltrasonicPort s_echo;
static ADCChannel s_adc;
static StepMotor s_step_motor;
static StepperController s_stepper;
static GpioOutput s_led;
static GpioButton s_button;
static uint8_t s_adc_pending;
static uint8_t s_led_on;

static void fail(uint32_t code)
{
    g_all_error=code;
    g_all_state=0xDEADu;
}

/* S32DS project's ACTUAL FTM channel-pair IRQ must forward here.
 * This function is not a vector handler and does not enable NVIC itself. */
void MP_TERP_AllTestEchoCaptureIrq(void)
{
    if(g_all_state==1u||g_all_state==2u)
        UltrasonicPort_OnCaptureIrq(&s_echo);
}

/* Same rule: actual PORTx IRQ must forward here if button enabled. */
void MP_TERP_AllTestButtonPortIrq(void)
{
    if((g_all_state==1u||g_all_state==2u) && s_cfg.use_gpio_button)
        GpioButton_OnPortIrq(&s_button);
}

static bool validate_mapping(void)
{
    if(s_cfg.timer.tick_channel==TIMER_CHANNEL_NONE ||
       s_cfg.timer.clock_hz==0u ||
       s_cfg.led_gpio.port==NULL || s_cfg.led_gpio.gpio==NULL ||
       s_cfg.button_gpio.port==NULL || s_cfg.button_gpio.gpio==NULL ||
       s_cfg.servo.port==NULL || s_cfg.ultrasonic.echo_port==NULL ||
       s_cfg.ultrasonic.trigger_port==NULL ||
       s_cfg.ultrasonic.trigger_gpio==NULL ||
       s_cfg.adc.adc==NULL ||
       s_cfg.stepper.period_us==0u ||
       s_cfg.stepper.steps_per_revolution==0u ||
       s_cfg.stepper_pins[0].port==NULL ||
       s_cfg.stepper_pins[1].port==NULL ||
       s_cfg.stepper_pins[2].port==NULL ||
       s_cfg.stepper_pins[3].port==NULL ||
       s_cfg.servo.ftm==NULL || s_cfg.ultrasonic.ftm==NULL ||
       s_cfg.servo.ftm==s_cfg.ultrasonic.ftm ||
       s_cfg.timer.tick_channel==s_cfg.stepper.timer_channel ||
       s_cfg.timer.tick_channel==s_cfg.ultrasonic.trigger_timer_channel ||
       s_cfg.stepper.timer_channel==s_cfg.ultrasonic.trigger_timer_channel ||
       s_cfg.stepper_delay==NULL || !s_cfg.use_gpio_led ||
       !s_cfg.use_gpio_button || s_cfg.lcd_write==NULL)
        return false;
    return true;
}

static void draw_screen(void)
{
    char line[32];
    OLED_Clear();
    OLED_DrawString(0,0,"MP_TERP ALL TEST",1u,OLED_WHITE);
    (void)snprintf(line,sizeof(line),"Echo %lu mm",
                   (unsigned long)g_all_range_mm);
    OLED_DrawString(0,12,line,1u,OLED_WHITE);
    (void)snprintf(line,sizeof(line),"ADC %lu",
                   (unsigned long)g_all_adc_raw);
    OLED_DrawString(0,24,line,1u,OLED_WHITE);
    (void)snprintf(line,sizeof(line),"Servo %ld",
                   (long)(g_all_servo_applied_mdeg/1000));
    OLED_DrawString(0,36,line,1u,OLED_WHITE);
    OLED_DrawString(0,48,g_all_stepper_homed?"STEP HOME":"STEP NOT HOME",
                    1u,OLED_WHITE);
    if(OLED_Update()==0)++g_all_oled_updates;
    else g_all_error=81u;
}

static void draw_lcd(void)
{
    char row[17];
    (void)snprintf(row,sizeof(row),"Echo:%5lumm",
                   (unsigned long)g_all_range_mm);
    if(!s_cfg.lcd_write(s_cfg.user,0u,row))g_all_error=82u;
    (void)snprintf(row,sizeof(row),"ADC:%4lu S:%3ld",
                   (unsigned long)g_all_adc_raw,
                   (long)(g_all_servo_applied_mdeg/1000));
    if(!s_cfg.lcd_write(s_cfg.user,1u,row))g_all_error=83u;
    ++g_all_lcd_updates;
}

int main(void)
{
    uint32_t last_echo=0u,last_display=0u,last_led=0u;
    g_all_state=0u;
    g_all_error=0u;
#ifndef MP_TERP_CONFIRM_PINMAP
    /* Do not access peripherals until the actual board wiring is verified. */
    fail(3u);
#else
    if(!MP_TERP_GetBoardConfig(&s_cfg))fail(1u);
    else if(!validate_mapping())fail(2u);
    else {
        SOSC_init_8MHz();
        SPLL_init_160MHz();
        NormalRUNmode_80MHz();
        if(TimerManager_Init(&s_cfg.timer)!=TIMER_OK)fail(10u);
        else if(!GpioOutput_Init(&s_led,&s_cfg.led_gpio))fail(11u);
        else if(!GpioButton_Init(&s_button,&s_cfg.button_gpio))fail(12u);
        else if(!ADCChannel_Init(&s_adc,&s_cfg.adc))fail(13u);
        else if(StepMotor_Init(&s_step_motor,s_cfg.stepper_pins,
                               s_cfg.stepper_delay)!=0)fail(14u);
        else {
            StepperControllerConfig stepcfg=s_cfg.stepper;
            stepcfg.motor=&s_step_motor;
            StepMotor_SetMode(&s_step_motor,s_cfg.stepper_mode);
            if(StepperController_Init(&s_stepper,&stepcfg)!=STEPPER_OK)fail(15u);
            else if(Servo_Init(&s_servo,&s_cfg.servo)!=SERVO_OK)fail(16u);
            else if(!UltrasonicPort_Init(&s_echo,&s_cfg.ultrasonic))fail(17u);
            else if(s_cfg.custom_oled_port &&
                    !OLED_PortConfigure(&s_cfg.oled_port))fail(18u);
            else if(OLED_Init()!=0)fail(19u);
            else if(!MP_TERP_AllTestLCDInit())fail(20u);
            else {
                g_all_servo_request_mdeg=s_cfg.servo.initial_angle_mdeg;
                g_all_servo_applied_mdeg=s_cfg.servo.initial_angle_mdeg;
                g_all_stepper_home_confirm=0u;
                g_all_stepper_home_angle_mdeg=0;
                g_all_stepper_request_mdeg=0;
                g_all_stepper_applied_mdeg=0;
                s_adc_pending=0u;s_led_on=0u;
                g_all_state=1u;
            }
        }
    }

#endif /* MP_TERP_CONFIRM_PINMAP */

    for(;;) {
        uint32_t now,mm;
        bool valid;
        uint16_t raw;
        int32_t servo_target,step_target,angle;
        bool moving;
        if(g_all_state==0xDEADu)continue;
        now=TimerManager_GetTickMs();
        g_all_tick_ms=now;
        UltrasonicPort_Update(&s_echo,now);
        if(UltrasonicPort_Take(&s_echo,&mm,&valid)) {
            ++g_all_echo_samples;
            if(valid) {
                ++g_all_echo_valid;
                g_all_range_mm=mm;
            } else {
                ++g_all_echo_timeout;
                g_all_range_mm=0u;
            }
        }
        if(s_echo.sensor.state==US_IDLE &&
           (uint32_t)(now-last_echo)>=100u) {
            if(UltrasonicPort_Start(&s_echo,now))last_echo=now;
            else g_all_error=31u;
        }

        if(!s_adc_pending) {
            if(ADCChannel_Start(&s_adc))s_adc_pending=1u;
        } else if(ADCChannel_Poll(&s_adc,&raw)) {
            g_all_adc_raw=raw;
            s_adc_pending=0u;
        }

        if(GpioButton_TakePress(&s_button))++g_all_button_presses;

        servo_target=g_all_servo_request_mdeg;
        if(servo_target!=g_all_servo_applied_mdeg) {
            if(Servo_SetAngle(&s_servo,servo_target)==SERVO_OK)
                g_all_servo_applied_mdeg=servo_target;
            else {
                g_all_error=41u;
                g_all_servo_request_mdeg=g_all_servo_applied_mdeg;
            }
        }

        /* Explicit home token, not automatic. The stepper must physically
         * be at the angle supplied in g_all_stepper_home_angle_mdeg. */
        if(g_all_stepper_home_confirm==0x484F4D45u && !g_all_stepper_homed) {
            if(StepperController_SetHome(&s_stepper,
                    g_all_stepper_home_angle_mdeg)==STEPPER_OK) {
                g_all_stepper_homed=1u;
                g_all_stepper_applied_mdeg=g_all_stepper_home_angle_mdeg;
                g_all_stepper_request_mdeg=g_all_stepper_home_angle_mdeg;
            } else g_all_error=51u;
            g_all_stepper_home_confirm=0u;
        }
        if(g_all_stepper_homed) {
            step_target=g_all_stepper_request_mdeg;
            if(step_target!=g_all_stepper_applied_mdeg) {
                if(StepperController_SetTargetAngle(&s_stepper,step_target)
                   ==STEPPER_OK)
                    g_all_stepper_applied_mdeg=step_target;
                else {
                    g_all_error=52u;
                    g_all_stepper_request_mdeg=g_all_stepper_applied_mdeg;
                }
            }
            if(StepperController_GetPosition(&s_stepper,&angle,NULL,&moving)
                ==STEPPER_OK)g_all_stepper_moving=moving?1u:0u;
        }

        if((uint32_t)(now-last_led)>=500u) {
            last_led=now;
            s_led_on=(uint8_t)!s_led_on;
            GpioOutput_Write(&s_led,s_led_on!=0u);
            ++g_all_heartbeat;
        }
        if((uint32_t)(now-last_display)>=300u) {
            last_display=now;
            draw_screen();
            draw_lcd();
        }
        g_all_state=2u;
    }
}
#endif /* MP_TERP_ALL_TEST_MAIN */
