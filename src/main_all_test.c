#include "project_config.h"

/*
 * Full-peripheral bring-up on REAL hardware, without false 2-D radar angles.
 *
 * Build with MP_TERP_ALL_TEST_MAIN (and not MP_TERP_STAGE_MAIN).
 * Choose verified devices in project_config.h before building.
 * Each selected device is initialized independently.
 *
 * All inputs/displays and both motors' driver interfaces are initialized:
 * - Servo holds its configured initial angle until debugger command.
 * - Ultrasonic measures fixed-direction distance every ~100ms.
 * - ADC potentiometer read continuously; OLED/LCD display raw diagnostics.
 * - Standalone stepper jog: one button-triggered bounded round trip per reset.
 *   Legacy manual home/target mode remains available when jog is disabled.
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
#include "mp_irq_compat.h"

extern bool MP_TERP_GetBoardConfig(BoardSystemConfig *cfg);
extern bool MP_TERP_AllTestLCDInit(void);

/* Mask: LED=1 ADC=2 BUTTON=4 OLED=8 LCD=16 ECHO=32 SERVO=64 STEP=128. */
enum { DEV_LED=1u, DEV_ADC=2u, DEV_BUTTON=4u, DEV_OLED=8u,
       DEV_LCD=16u, DEV_ECHO=32u, DEV_SERVO=64u, DEV_STEP=128u };
volatile uint32_t g_all_enabled_mask;
volatile uint32_t g_all_ready_mask;
volatile uint32_t g_all_failed_mask;
volatile uint32_t g_all_runtime_failed_mask;
volatile uint32_t g_all_state;        /* 0 boot, 2 running, 0xDEAD fatal failure */
volatile uint32_t g_all_error;
volatile uint32_t g_all_boot_stage;
volatile uint32_t g_all_wdog_cs_before;
volatile uint32_t g_all_wdog_cs_after;
volatile uint32_t g_all_exception_number;
volatile uint32_t g_all_fault_cfsr;
volatile uint32_t g_all_fault_hfsr;
volatile uint32_t g_all_fault_icsr;
volatile uint32_t g_all_fault_mmfar;
volatile uint32_t g_all_fault_bfar;
volatile uint32_t g_all_fault_shcsr;

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
volatile uint32_t g_all_servo_jog_state; /* 0 off, 1 release, 2 armed, 3 left, 4 right, 5 center, 6 done, 7 failed */
volatile uint32_t g_all_sw3_presses;
volatile uint32_t g_all_motor_test_busy; /* 0 idle, 1 stepper, 2 servo */
volatile uint32_t g_all_stepper_jog_state; /* 0 disabled, 1 waiting release, 2 armed, 3 out, 4 dwell, 5 back, 6 done, 7 failed */
volatile int32_t g_all_stepper_jog_steps; /* commanded step count, not encoder feedback */

static BoardSystemConfig s_cfg;
static Servo s_servo;
static UltrasonicPort s_echo;
static ADCChannel s_adc;
static StepMotor s_step_motor;
static StepperController s_stepper;
static GpioOutput s_led;
static GpioButton s_button;
static GpioButton s_button3;
static bool s_button3_ready;
static uint8_t s_adc_pending;
static uint8_t s_led_on;

static void fail(uint32_t code)
{
    g_all_error=code;
    g_all_state=0xDEADu;
}

/* Cortex-M4 fault capture. No SDK-specific CMSIS SCB declaration required.
 * Do not resume a faulted context. Inspect these values and reset.
 * MMFAR/BFAR are meaningful only when their CFSR validity flags are set.
 */
#if defined(__arm__) || defined(__thumb__)
static void capture_core_fault(uint32_t code)
{
    uint32_t ipsr;
    __asm volatile ("mrs %0, ipsr" : "=r"(ipsr));
    g_all_exception_number=ipsr;
    g_all_fault_icsr=*(volatile const uint32_t *)0xE000ED04u;
    g_all_fault_shcsr=*(volatile const uint32_t *)0xE000ED24u;
    g_all_fault_cfsr=*(volatile const uint32_t *)0xE000ED28u;
    g_all_fault_hfsr=*(volatile const uint32_t *)0xE000ED2Cu;
    g_all_fault_mmfar=*(volatile const uint32_t *)0xE000ED34u;
    g_all_fault_bfar=*(volatile const uint32_t *)0xE000ED38u;
    fail(code);
    for(;;) { __asm volatile ("nop"); }
}
void HardFault_Handler(void) { capture_core_fault(92u); }
void MemManage_Handler(void) { capture_core_fault(93u); }
void BusFault_Handler(void) { capture_core_fault(94u); }
void UsageFault_Handler(void) { capture_core_fault(95u); }
#endif

/* Bench only: disable WDOG before clock changes. Preserve the prior
 * interrupt mask. A locked configuration is reported rather than ignored.
 * The default S32K144 CMD32EN mode uses the NXP 32-bit unlock sequence.
 */
static bool bench_wdog_disable(void)
{
    uint32_t mask=MP_IrqSave();
    g_all_wdog_cs_before=WDOG->CS;
    if((WDOG->CS & WDOG_CS_EN_MASK)!=0u) {
        if((WDOG->CS & WDOG_CS_CMD32EN_MASK)!=0u) {
            WDOG->CNT=0xD928C520u;
        } else {
            volatile uint16_t *cnt=(volatile uint16_t *)&WDOG->CNT;
            *cnt=0xC520u;
            *cnt=0xD928u;
        }
        for(uint32_t i=0u;i<10000u;i++) {
            if((WDOG->CS & WDOG_CS_ULK_MASK)!=0u)break;
        }
        if((WDOG->CS & WDOG_CS_ULK_MASK)!=0u) {
            WDOG->TOVAL=0xFFFFu;
            WDOG->CS=0x2100u;
            for(uint32_t i=0u;i<10000u;i++) {
                if((WDOG->CS & WDOG_CS_RCS_MASK)!=0u)break;
            }
        }
    }
    g_all_wdog_cs_after=WDOG->CS;
    bool ok=(g_all_wdog_cs_after & WDOG_CS_EN_MASK)==0u;
    MP_IrqRestore(mask);
    return ok;
}

/* Capture an unexpected watchdog interrupt; never return into an IRQ storm.
 * No external IO is authorized during the initial bench stage.
 */
void WDOG_EWM_IRQHandler(void)
{
#if defined(__arm__) || defined(__thumb__)
    uint32_t ipsr;
    __asm volatile ("mrs %0, ipsr" : "=r"(ipsr));
    g_all_exception_number=ipsr;
#endif
    g_all_wdog_cs_after=WDOG->CS;
    fail(91u);
    for(;;){}
}

/* S32DS project's ACTUAL FTM channel-pair IRQ must forward here.
 * This function is not a vector handler and does not enable NVIC itself. */
void MP_TERP_AllTestEchoCaptureIrq(void)
{
    if((g_all_ready_mask&DEV_ECHO)!=0u)
        UltrasonicPort_OnCaptureIrq(&s_echo);
}

/* Same rule: actual PORTx IRQ must forward here if button enabled. */
void MP_TERP_AllTestButtonPortIrq(void)
{
    if((g_all_ready_mask&DEV_BUTTON)!=0u)
        GpioButton_OnPortIrq(&s_button);
    if(s_button3_ready)GpioButton_OnPortIrq(&s_button3);
}

/* Record ordinary driver init failures without stopping independent devices. */
static void device_result(uint32_t bit, bool ok, uint32_t code)
{
    if(ok) g_all_ready_mask |= bit;
    else { g_all_failed_mask |= bit; g_all_error=code; }
}
static void runtime_error(uint32_t bit,uint32_t code)
{
    g_all_runtime_failed_mask |= bit;
    g_all_error=code;
}

#if TEST_STEPPER && TEST_STEPPER_BUTTON_JOG
/* Unloaded standalone bench test. Relative zero only; never physical homing.
 * Require a stable released button after boot, then a stable press.
 * A single bounded 100-degree command per reset avoids accumulated open-loop position error.
 */
static uint32_t s_jog_since;
static uint32_t s_button_since;
static bool s_button_last;
static bool s_button_sampled;

static void jog_fail(uint32_t code)
{
    (void)StepperController_Stop(&s_stepper);
    (void)StepperController_Release(&s_stepper);
    g_all_stepper_moving=0u;
    g_all_stepper_jog_state=7u;
    g_all_motor_test_busy=0u;
    runtime_error(DEV_STEP,code);
}

static void stepper_jog_update(uint32_t now)
{
    bool pressed, moving=false;
    int32_t steps=0;
    uint32_t state=g_all_stepper_jog_state;
    if(state==0u || state>=6u)return;
    if(!(g_all_ready_mask&DEV_BUTTON)) { jog_fail(53u); return; }
    pressed=((s_cfg.button_gpio.gpio->PDIR>>s_cfg.button_gpio.pin)&1u)!=0u;
    if(s_cfg.button_gpio.active_low)pressed=!pressed;
    if(!s_button_sampled || pressed!=s_button_last) {
        s_button_sampled=true;
        s_button_last=pressed;
        s_button_since=now;
    }
    if(state==1u) {
        if(!pressed && (uint32_t)(now-s_button_since)>=100u)
            g_all_stepper_jog_state=2u;
        return;
    }
    if(state==2u) {
        if(!pressed || (uint32_t)(now-s_button_since)<100u)return;
        if(g_all_motor_test_busy!=0u) { g_all_stepper_jog_state=1u;return; }
        g_all_motor_test_busy=1u;
        if(StepperController_SetHome(&s_stepper,0)!=STEPPER_OK ||
           StepperController_SetTargetAngle(&s_stepper,TEST_STEPPER_TARGET_MDEG)!=STEPPER_OK) {
            jog_fail(54u);return;
        }
        s_jog_since=now;
        g_all_stepper_jog_state=3u;
        g_all_stepper_moving=1u;
        return;
    }
    if(StepperController_GetPosition(&s_stepper,NULL,&steps,&moving)!=STEPPER_OK) {
        jog_fail(55u);return;
    }
    g_all_stepper_jog_steps=steps;
    g_all_stepper_moving=moving?1u:0u;
    if(state==3u || state==5u) {
        if(moving) {
            if((uint32_t)(now-s_jog_since)>=TEST_STEPPER_TIMEOUT_MS)jog_fail(56u);
            return;
        }
        if(state==3u) {
            if(steps<=0) { jog_fail(57u);return; }
            s_jog_since=now;
            g_all_stepper_jog_state=4u;
        } else {
            if(steps!=0 || StepperController_Release(&s_stepper)!=STEPPER_OK) {
                jog_fail(57u);return;
            }
            g_all_stepper_jog_state=6u;
            g_all_motor_test_busy=0u;
        }
    } else if(state==4u && (uint32_t)(now-s_jog_since)>=500u) {
        if(StepperController_SetTargetAngle(&s_stepper,0)!=STEPPER_OK) {
            jog_fail(58u);return;
        }
        s_jog_since=now;
        g_all_stepper_jog_state=5u;
    }
}
#endif

#if TEST_SERVO && TEST_BUTTON
/* Standalone unloaded SG90 only. 90 -> 40 -> 140 -> 90 degrees.
 * The 40-to-140 sweep is 100 commanded degrees, not encoder feedback.
 * No scan or sensor-to-motor coupling. */
static uint32_t s_servo_since,s_sw3_since;
static bool s_sw3_sampled,s_sw3_last;
static bool servo_jog_command(int32_t angle)
{
    if(Servo_SetAngle(&s_servo,angle)!=SERVO_OK) {
        runtime_error(DEV_SERVO,42u);
        g_all_servo_jog_state=7u;
        g_all_motor_test_busy=0u;
        return false;
    }
    g_all_servo_request_mdeg=angle;
    g_all_servo_applied_mdeg=angle;
    return true;
}
static void servo_jog_update(uint32_t now)
{
    uint32_t state=g_all_servo_jog_state;
    bool pressed;
    if(state==0u || state>=6u)return;
    if(!s_button3_ready) { g_all_servo_jog_state=7u;runtime_error(DEV_SERVO,43u);return; }
    pressed=((s_button3.config.gpio->PDIR>>s_button3.config.pin)&1u)!=0u;
    if(s_button3.config.active_low)pressed=!pressed;
    if(!s_sw3_sampled || pressed!=s_sw3_last) {
        s_sw3_sampled=true;s_sw3_last=pressed;s_sw3_since=now;
    }
    if(state==1u) {
        if(!pressed && (uint32_t)(now-s_sw3_since)>=100u)g_all_servo_jog_state=2u;
    } else if(state==2u) {
        if(!pressed || (uint32_t)(now-s_sw3_since)<100u)return;
        if(g_all_motor_test_busy!=0u) { g_all_servo_jog_state=1u;return; }
        g_all_motor_test_busy=2u;
        if(servo_jog_command(TEST_SERVO_LEFT_MDEG)) {
            s_servo_since=now;g_all_servo_jog_state=3u;
        }
    } else if(state==3u && (uint32_t)(now-s_servo_since)>=TEST_SERVO_HOLD_MS) {
        if(servo_jog_command(TEST_SERVO_RIGHT_MDEG)) {
            s_servo_since=now;g_all_servo_jog_state=4u;
        }
    } else if(state==4u && (uint32_t)(now-s_servo_since)>=TEST_SERVO_HOLD_MS) {
        if(servo_jog_command(TEST_SERVO_CENTER_MDEG)) {
            s_servo_since=now;g_all_servo_jog_state=5u;
        }
    } else if(state==5u && (uint32_t)(now-s_servo_since)>=TEST_SERVO_HOLD_MS) {
        g_all_servo_jog_state=6u;g_all_motor_test_busy=0u;
    }
}
#endif

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
    (void)snprintf(line,sizeof(line),"STEP %lu SERVO %lu",
        (unsigned long)g_all_stepper_jog_state,(unsigned long)g_all_servo_jog_state);
    OLED_DrawString(0,48,line,1u,OLED_WHITE);
    if(OLED_Update()==0)++g_all_oled_updates;
    else runtime_error(DEV_OLED,81u);
}

static void draw_lcd(void)
{
    char row[17];
    (void)snprintf(row,sizeof(row),"Echo:%5lumm",
                   (unsigned long)g_all_range_mm);
    bool ok=s_cfg.lcd_write(s_cfg.user,0u,row);
    if(!ok)runtime_error(DEV_LCD,82u);
    (void)snprintf(row,sizeof(row),"ADC:%4lu S:%3ld",
                   (unsigned long)g_all_adc_raw,
                   (long)(g_all_servo_applied_mdeg/1000));
    if(!s_cfg.lcd_write(s_cfg.user,1u,row))runtime_error(DEV_LCD,83u);
    else if(ok)++g_all_lcd_updates;
}

int main(void)
{
    uint32_t last_echo=0u,last_display=0u,last_led=0u;
    g_all_state=0u;
    g_all_error=0u;
    g_all_boot_stage=1u;
    g_all_enabled_mask=(TEST_LED?DEV_LED:0u)|(TEST_ADC?DEV_ADC:0u)|
        (TEST_BUTTON?DEV_BUTTON:0u)|(TEST_OLED?DEV_OLED:0u)|
        (TEST_LCD?DEV_LCD:0u)|(TEST_ULTRASONIC?DEV_ECHO:0u)|
        (TEST_SERVO?DEV_SERVO:0u)|(TEST_STEPPER?DEV_STEP:0u);
    g_all_ready_mask=0u;g_all_failed_mask=0u;g_all_runtime_failed_mask=0u;
    if(!bench_wdog_disable()) { fail(90u); for(;;){} }
    g_all_boot_stage=2u;
    if(!MP_TERP_GetBoardConfig(&s_cfg)) { fail(1u); for(;;){} }
    g_all_boot_stage=3u;
    SOSC_init_8MHz();
    g_all_boot_stage=4u;
    SPLL_init_160MHz();
    g_all_boot_stage=5u;
    NormalRUNmode_80MHz();
    g_all_boot_stage=6u;
    if(TimerManager_Init(&s_cfg.timer)!=TIMER_OK) { fail(10u); for(;;){} }

    /* Initialization is independent. Disabled devices are not accessed. */
    g_all_boot_stage=7u;
    if(TEST_LED) device_result(DEV_LED,GpioOutput_Init(&s_led,&s_cfg.led_gpio),11u);
    if(TEST_ADC) device_result(DEV_ADC,ADCChannel_Init(&s_adc,&s_cfg.adc),13u);
    if(TEST_BUTTON) {
        device_result(DEV_BUTTON,GpioButton_Init(&s_button,&s_cfg.button_gpio),12u);
        if(g_all_ready_mask&DEV_BUTTON) {
            GpioButtonConfig sw3=s_cfg.button_gpio;
            sw3.pin=13u; /* EVB SW3, same port and polarity as SW2 */
            s_button3_ready=GpioButton_Init(&s_button3,&sw3);
            if(!s_button3_ready)runtime_error(DEV_BUTTON,44u);
            MP_NvicClearPending(PORTC_IRQn);
            MP_NvicEnable(PORTC_IRQn);
        }
    }
    g_all_boot_stage=8u;
    if(TEST_ULTRASONIC) {
        bool ok=s_cfg.ultrasonic.trigger_timer_channel!=s_cfg.timer.tick_channel &&
            (!TEST_SERVO || s_cfg.ultrasonic.ftm!=s_cfg.servo.ftm) &&
            UltrasonicPort_Init(&s_echo,&s_cfg.ultrasonic);
        device_result(DEV_ECHO,ok,17u);
        if(ok) {
            MP_NvicClearPending(FTM0_Ch6_Ch7_IRQn);
            MP_NvicEnable(FTM0_Ch6_Ch7_IRQn);
        }
    }
    if(TEST_OLED) {
        bool ok=!s_cfg.custom_oled_port || OLED_PortConfigure(&s_cfg.oled_port);
        if(!ok)device_result(DEV_OLED,false,18u);
        else device_result(DEV_OLED,OLED_Init()==0,19u);
    }
    if(TEST_LCD) device_result(DEV_LCD,s_cfg.lcd_write!=NULL && MP_TERP_AllTestLCDInit(),20u);
    if(TEST_SERVO) {
        bool ok=(!TEST_ULTRASONIC || s_cfg.servo.ftm!=s_cfg.ultrasonic.ftm) &&
            Servo_Init(&s_servo,&s_cfg.servo)==SERVO_OK;
        device_result(DEV_SERVO,ok,16u);
        if(ok) {
            g_all_servo_request_mdeg=s_cfg.servo.initial_angle_mdeg;
            g_all_servo_applied_mdeg=s_cfg.servo.initial_angle_mdeg;
        }
    }
    if(TEST_STEPPER) {
        bool ok=s_cfg.stepper.timer_channel!=s_cfg.timer.tick_channel &&
            (!TEST_ULTRASONIC || s_cfg.stepper.timer_channel!=s_cfg.ultrasonic.trigger_timer_channel);
        if(!ok || StepMotor_Init(&s_step_motor,s_cfg.stepper_pins,s_cfg.stepper_delay)!=0)
            device_result(DEV_STEP,false,14u);
        else {
            StepperControllerConfig stepcfg=s_cfg.stepper;
            stepcfg.motor=&s_step_motor;
            StepMotor_SetMode(&s_step_motor,s_cfg.stepper_mode);
            ok=StepperController_Init(&s_stepper,&stepcfg)==STEPPER_OK;
            device_result(DEV_STEP,ok,15u);
            if(!ok)StepMotor_Release(&s_step_motor);
        }
    }
#if TEST_STEPPER && TEST_STEPPER_BUTTON_JOG
    if(g_all_ready_mask&DEV_STEP)g_all_stepper_jog_state=1u;
#endif
#if TEST_SERVO && TEST_BUTTON
    if(g_all_ready_mask&DEV_SERVO)g_all_servo_jog_state=1u;
#endif
    g_all_boot_stage=9u;
    g_all_state=2u;

    for(;;) {
        uint32_t now,mm;
        bool valid;
        uint16_t raw;
#if !(TEST_SERVO && TEST_BUTTON)
        int32_t servo_target;
#endif
#if !(TEST_STEPPER && TEST_STEPPER_BUTTON_JOG)
        int32_t step_target,angle;
        bool moving;
#endif
        if(g_all_state==0xDEADu)continue;
        now=TimerManager_GetTickMs();
        g_all_tick_ms=now;
        if(g_all_ready_mask&DEV_ECHO) {
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
            else runtime_error(DEV_ECHO,31u);
        }
        }

        if(g_all_ready_mask&DEV_ADC) {
        if(!s_adc_pending) {
            if(ADCChannel_Start(&s_adc))s_adc_pending=1u;
        } else if(ADCChannel_Poll(&s_adc,&raw)) {
            g_all_adc_raw=raw;
            s_adc_pending=0u;
        }

        }
        if((g_all_ready_mask&DEV_BUTTON) && GpioButton_TakePress(&s_button))++g_all_button_presses;

        if(s_button3_ready && GpioButton_TakePress(&s_button3))++g_all_sw3_presses;
        if(g_all_ready_mask&DEV_SERVO) {
#if TEST_SERVO && TEST_BUTTON
        servo_jog_update(now);
#else
        servo_target=g_all_servo_request_mdeg;
        if(servo_target!=g_all_servo_applied_mdeg) {
            if(Servo_SetAngle(&s_servo,servo_target)==SERVO_OK)
                g_all_servo_applied_mdeg=servo_target;
            else {
                runtime_error(DEV_SERVO,41u);
                g_all_servo_request_mdeg=g_all_servo_applied_mdeg;
            }
        }
#endif
        }
        if(g_all_ready_mask&DEV_STEP) {
#if TEST_STEPPER && TEST_STEPPER_BUTTON_JOG
        stepper_jog_update(now);
#else
        /* Explicit home token, not automatic. The stepper must physically
         * be at the angle supplied in g_all_stepper_home_angle_mdeg. */
        if(g_all_stepper_home_confirm==0x484F4D45u && !g_all_stepper_homed) {
            if(StepperController_SetHome(&s_stepper,
                    g_all_stepper_home_angle_mdeg)==STEPPER_OK) {
                g_all_stepper_homed=1u;
                g_all_stepper_applied_mdeg=g_all_stepper_home_angle_mdeg;
                g_all_stepper_request_mdeg=g_all_stepper_home_angle_mdeg;
            } else runtime_error(DEV_STEP,51u);
            g_all_stepper_home_confirm=0u;
        }
        if(g_all_stepper_homed) {
            step_target=g_all_stepper_request_mdeg;
            if(step_target!=g_all_stepper_applied_mdeg) {
                if(StepperController_SetTargetAngle(&s_stepper,step_target)
                   ==STEPPER_OK)
                    g_all_stepper_applied_mdeg=step_target;
                else {
                    runtime_error(DEV_STEP,52u);
                    g_all_stepper_request_mdeg=g_all_stepper_applied_mdeg;
                }
            }
            if(StepperController_GetPosition(&s_stepper,&angle,NULL,&moving)
                ==STEPPER_OK)g_all_stepper_moving=moving?1u:0u;
        }
#endif
        }
        if((g_all_ready_mask&DEV_LED) && (uint32_t)(now-last_led)>=500u) {
            last_led=now;
            s_led_on=(uint8_t)!s_led_on;
            GpioOutput_Write(&s_led,s_led_on!=0u);
            ++g_all_heartbeat;
        }
        if((uint32_t)(now-last_display)>=300u) {
            last_display=now;
            if(g_all_ready_mask&DEV_OLED)draw_screen();
            if(g_all_ready_mask&DEV_LCD)draw_lcd();
        }
        g_all_state=2u;
    }
}
#endif /* MP_TERP_ALL_TEST_MAIN */
