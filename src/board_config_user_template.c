#include "project_config.h"

/* Provisional EVB-Q100 bench profile. Verify actual shield wiring first.
 * Enable MP_TERP_USE_BOARD_CONFIG_TEMPLATE and MP_TERP_CONFIRM_PINMAP.
 * Independent peripheral test only. No automatic tracking or motor sweep.
 */
#if defined(MP_TERP_USE_BOARD_CONFIG_TEMPLATE) && (defined(MP_TERP_ALL_TEST_MAIN) || (defined(MP_TERP_STAGE_MAIN) && MP_TERP_STAGE != 4))
#include "board_system.h"
#include "lcd_hd44780.h"
#include "mp_irq_compat.h"
#include <stddef.h>
#include <string.h>

static LCDHD44780 s_lcd;

/* LPIT tick provides a deliberately conservative delay >= requested time.
 * Foreground only; requires enabled interrupts and a working 1ms tick. */
static void delay_ms(uint32_t ms)
{
    uint32_t start=TimerManager_GetTickMs();
    while((uint32_t)(TimerManager_GetTickMs()-start)<=ms){}
}
static void lcd_delay_us(void *ctx,uint32_t us)
{
    (void)ctx;
    delay_ms((us+999u)/1000u);
}

bool MP_TERP_GetBoardConfig(BoardSystemConfig *cfg)
{
    if(!cfg)return false;
    memset(cfg,0,sizeof(*cfg));
#ifndef MP_TERP_CONFIRM_PINMAP
    return false;
#else
    cfg->timer=(TimerManagerConfig){6u,40000000u,TIMER_CH2};
    /* PTD10 FTM2_CH0 ALT2; separate from Echo FTM0. */
    cfg->servo=(ServoConfig){FTM2,0u,PORTD,PCC_PORTD_INDEX,10u,2u,
        1u,8000000u,20000u,1100u,1900u,20000,160000,90000};
    /* SG90 120-degree COMMAND range: 30/60/90/120/150 degrees.
     * Baseline pulses reproduce the previously used 20..160 linear PWM map.
     * These are PROVISIONAL, NOT measured angular corrections. After measuring
     * actual angles, replace ONLY the pulse_us values here. Preserve rising
     * pulse order and the conservative 1100..1900us hardware envelope.
     * Calibrated Servo_SetAngle rejects angles outside 30..150 degrees. */
    cfg->servo_calibration_count=5u;
    cfg->servo_calibration[0]=(ServoCalibrationPoint){30000,1157u};
    cfg->servo_calibration[1]=(ServoCalibrationPoint){60000,1329u};
    cfg->servo_calibration[2]=(ServoCalibrationPoint){90000,1500u};
    cfg->servo_calibration[3]=(ServoCalibrationPoint){120000,1671u};
    cfg->servo_calibration[4]=(ServoCalibrationPoint){150000,1843u};
    cfg->ultrasonic=(UltrasonicPortConfig){
        FTM0,6u,1u,8000000u,PORTE,PCC_PORTE_INDEX,8u,2u,
        PORTB,PTB,PCC_PORTB_INDEX,8u,TIMER_CH1,10u,30000u,100u,25000u};
    cfg->stepper_pins[0]=(StepMotorPin){PORTB,PTB,PCC_PORTB_INDEX,9u};
    cfg->stepper_pins[1]=(StepMotorPin){PORTB,PTB,PCC_PORTB_INDEX,10u};
    cfg->stepper_pins[2]=(StepMotorPin){PORTB,PTB,PCC_PORTB_INDEX,11u};
    cfg->stepper_pins[3]=(StepMotorPin){PORTB,PTB,PCC_PORTB_INDEX,5u};
    cfg->stepper_delay=delay_ms;
    cfg->stepper_mode=STEP_MOTOR_HALF_STEP;
    /* Provisional 4096 half-steps/output rev; physical angle must be checked. */
    cfg->stepper=(StepperControllerConfig){NULL,TIMER_CH0,5000u,4096u,-110000,110000};
    cfg->adc=(ADCChannelConfig){ADC0,PCC_ADC0_INDEX,1u,12u,5000u};
    cfg->custom_oled_port=0u; /* existing PTA2/3 ALT3 */
    cfg->use_gpio_led=1u;
    cfg->led_gpio=(GpioOutputConfig){PORTD,PTD,PCC_PORTD_INDEX,15u,false};
    cfg->use_gpio_button=1u;
    cfg->button_gpio=(GpioButtonConfig){PORTC,PTC,PCC_PORTC_INDEX,12u,false,false};
    cfg->lcd_write=LCDHD44780_WriteLine;
    cfg->user=&s_lcd;
    return true;
#endif
}

#if defined(MP_TERP_ALL_TEST_MAIN)
extern void MP_TERP_AllTestEchoCaptureIrq(void);
extern void MP_TERP_AllTestButtonPortIrq(void);

bool MP_TERP_AllTestLCDInit(void)
{
    LCDHD44780Config cfg={0};
    cfg.rs=(LCDPin){PORTD,PTD,PCC_PORTD_INDEX,4u};
    cfg.enable=(LCDPin){PORTD,PTD,PCC_PORTD_INDEX,5u};
    cfg.data[0]=(LCDPin){PORTD,PTD,PCC_PORTD_INDEX,11u};
    cfg.data[1]=(LCDPin){PORTD,PTD,PCC_PORTD_INDEX,12u};
    cfg.data[2]=(LCDPin){PORTD,PTD,PCC_PORTD_INDEX,13u};
    cfg.data[3]=(LCDPin){PORTD,PTD,PCC_PORTD_INDEX,14u};
    cfg.columns=16u;cfg.rows=2u;cfg.delay_us=lcd_delay_us;
    return LCDHD44780_Init(&s_lcd,&cfg);
}
void FTM0_Ch6_Ch7_IRQHandler(void){MP_TERP_AllTestEchoCaptureIrq();}
void PORTC_IRQHandler(void){MP_TERP_AllTestButtonPortIrq();}
#elif MP_TERP_STAGE == 3
extern void MP_TERP_StageEchoCaptureIrq(void);
void FTM0_Ch6_Ch7_IRQHandler(void){MP_TERP_StageEchoCaptureIrq();}
#endif
#endif
