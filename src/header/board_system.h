#ifndef BOARD_SYSTEM_H
#define BOARD_SYSTEM_H
#include <stdbool.h>
#include <stdint.h>
#include "app.h"
#include "adc_channel.h"
#include "servo.h"
#include "stepper_controller.h"
#include "ultrasonic_port_s32k144.h"
#include "timer_manager.h"

typedef struct {
    TimerManagerConfig timer;
    ServoConfig servo;
    StepMotorPin stepper_pins[4];
    StepMotorDelayMs stepper_delay;
    StepMotorMode stepper_mode;
    StepperControllerConfig stepper;
    UltrasonicPortConfig ultrasonic;
    ADCChannelConfig adc;
    AppConfig application;
    bool (*lcd_write)(void *user,uint8_t row,const char *line);
    void (*led_write)(void *user,bool on);
    void *user;
} BoardSystemConfig;

typedef struct {
    Servo servo;
    StepMotor step_motor;
    StepperController stepper;
    UltrasonicPort ultrasonic;
    ADCChannel adc;
    App app;
    BoardSystemConfig config;
    uint8_t adc_pending;
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
#endif
