#ifndef MP_TERP_PROJECT_CONFIG_H
#define MP_TERP_PROJECT_CONFIG_H

/* Default bench entry point. No S32DS -D options required.
 * Explicit legacy stage/example selection still overrides this default.
 * Set MP_TERP_SMOKE_MAIN to return to the no-peripheral test.
 */
#if !defined(MP_TERP_SMOKE_MAIN) && !defined(MP_TERP_STAGE_MAIN) && !defined(MP_TERP_USE_EXAMPLE_MAIN)
#ifndef MP_TERP_ALL_TEST_MAIN
#define MP_TERP_ALL_TEST_MAIN
#endif
#endif

#if defined(MP_TERP_ALL_TEST_MAIN)
#ifndef MP_TERP_USE_BOARD_CONFIG_TEMPLATE
#define MP_TERP_USE_BOARD_CONFIG_TEMPLATE
#endif
/* Bench profile selected by request. Actual wiring is NOT certified.
 * Keep external loads disconnected until the documented pinout and voltage
 * are checked. Select each verified device below before building.
 */
#ifndef MP_TERP_CONFIRM_PINMAP
#define MP_TERP_CONFIRM_PINMAP
#endif
/* 1 = enable this peripheral, 0 = leave it untouched.
 * Full bench profile: verify wiring, supply and unloaded motor setup before powering.
 */
#define TEST_LED          1
#define TEST_ADC          1
#define TEST_BUTTON       1
#define TEST_OLED         1
#define TEST_LCD          1
#define TEST_ULTRASONIC   1
#define TEST_SERVO        1
#define TEST_STEPPER      1
/* One press per motor, per reset; keep mechanisms disconnected.
 * These are commanded angles, not measured encoder positions.
 * SW2: Stepper 0 -> 100 -> 0 degrees (at 4096 half-steps/rev).
 * SW3: SG90 90 -> 40 -> 140 -> 90 degrees (100-degree commanded span).
 * Stop immediately if an end stop, stall, abnormal noise or heat occurs.
 */
#define TEST_STEPPER_BUTTON_JOG 1
#define TEST_STEPPER_TARGET_MDEG 100000
#define TEST_STEPPER_TIMEOUT_MS  12000u
#define TEST_SERVO_LEFT_MDEG     40000
#define TEST_SERVO_RIGHT_MDEG    140000
#define TEST_SERVO_CENTER_MDEG   90000
#define TEST_SERVO_HOLD_MS       1500u
#if (TEST_SERVO_RIGHT_MDEG - TEST_SERVO_LEFT_MDEG) <= 90000
#error "Servo test must command more than 90 degrees of travel"
#endif
#if TEST_STEPPER_TARGET_MDEG <= 90000
#error "Stepper test must command more than 90 degrees of travel"
#endif
#if TEST_STEPPER && TEST_STEPPER_BUTTON_JOG && !TEST_BUTTON
#error "Stepper button jog requires button enabled"
#endif
#endif
#endif
