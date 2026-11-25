#ifndef BOARD_SYSTEM_H
#define BOARD_SYSTEM_H
#include <stdbool.h>
#include <stdint.h>
#include "app.h"
#include "adc_channel.h"
#include "gpio_io.h"
#include "servo.h"
#include "oled_port_s32k144.h"
#include "stepper_controller.h"
#include "ultrasonic_port_s32k144.h"
#include "timer_manager.h"

typedef struct {
    TimerManagerConfig timer;
    ServoConfig servo;
    /* Optional, board-specific servo command calibration (0=legacy map).
     * Profile is copied into Servo at initialization, never read via pointer. */
    ServoCalibrationPoint servo_calibration[SERVO_MAX_CALIBRATION_POINTS];
    uint8_t servo_calibration_count;
    StepMotorPin stepper_pins[4];
    StepMotorDelayMs stepper_delay;
    StepMotorMode stepper_mode;
    StepperControllerConfig stepper;
    UltrasonicPortConfig ultrasonic;
    ADCChannelConfig adc;
    OLEDPortConfig oled_port;
    uint8_t custom_oled_port; /* 0 keeps existing default mapping */
    AppConfig application;
    /* Scan angle corresponding to the Stepper's manually confirmed zero. */
    int32_t aiming_scan_center_mdeg;
    bool (*lcd_init)(void *user);
    bool (*lcd_write)(void *user,uint8_t row,const char *line);
    void (*led_write)(void *user,bool on);
    uint8_t use_gpio_led;
    GpioOutputConfig led_gpio;
    uint8_t use_gpio_button;
    GpioButtonConfig button_gpio;
    void *user;
} BoardSystemConfig;

typedef struct {
    Servo servo;
    StepMotor step_motor;
    StepperController stepper;
    UltrasonicPort ultrasonic;
    ADCChannel adc;
    GpioOutput led;
    GpioButton button;
    App app;
    BoardSystemConfig config;
    uint8_t adc_pending;
    int32_t aiming_scan_center_mdeg;
    uint8_t initialized;
} BoardSystem;

/* Uses existing clocks_and_modes.c and OLED I2C port driver. All pin/FTM
 * choices are supplied by BoardSystemConfig. S32DS linker/startup is supplied
 * by the user's actual MCU project, not by this portable repository. */
bool BoardSystem_Init(BoardSystem *board,const BoardSystemConfig *cfg);
void BoardSystem_Update(BoardSystem *board);
/* Connect selected FTM channel pair's IRQ to this hook. */
void BoardSystem_OnEchoCaptureIrq(BoardSystem *board);
void BoardSystem_OnFireButton(BoardSystem *board);
void BoardSystem_OnFireButtonPortIrq(BoardSystem *board);
/* Only after physically verifying the Stepper points at the selected scan
 * center. Declares relative zero; NO mechanical homing move is performed. */
bool BoardSystem_ConfirmStepperCenter(BoardSystem *board);
#endif
