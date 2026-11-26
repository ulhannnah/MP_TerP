#ifndef MP_TERP_PROJECT_CONFIG_H
#define MP_TERP_PROJECT_CONFIG_H

/* FINAL branch defaults to integrated Stage 5.
 * No geometry confirmation -> honest uncoupled diagnostic:
 * autonomous Servo sweep + fixed-direction Echo, no 2D target/Stepper aim.
 * Only after mechanically coupling the sensor with the SG90 may an S32DS
 * build explicitly define MP_TERP_CONFIRM_COUPLED_GEOMETRY for real radar.
 * -DMP_TERP_ALL_TEST_MAIN and -DMP_TERP_SMOKE_MAIN preserve debug modes.
 */
#if !defined(MP_TERP_SMOKE_MAIN) && !defined(MP_TERP_STAGE_MAIN) && \
    !defined(MP_TERP_ALL_TEST_MAIN) && !defined(MP_TERP_USE_EXAMPLE_MAIN)
#define MP_TERP_STAGE_MAIN
#define MP_TERP_STAGE 5
#define MP_TERP_FINAL_DEFAULT 1
#endif

#if defined(MP_TERP_STAGE_MAIN) && MP_TERP_STAGE == 5
/* Preserve the 2026-11-25 confirmed code pin assignments; do NOT waive
 * the independent sensor/servo mechanical-coupling check. */
#ifndef MP_TERP_USE_BOARD_CONFIG_TEMPLATE
#define MP_TERP_USE_BOARD_CONFIG_TEMPLATE
#endif
#ifndef MP_TERP_CONFIRM_PINMAP
#define MP_TERP_CONFIRM_PINMAP
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
 * Commands are NOT measured angles. SW2: Stepper 0 -> 100 -> 0 deg.
 * SW3: SG90 calibration walk 30 -> 60 -> 90 -> 120 -> 150 -> 90 deg.
 * Span is 120 COMMAND degrees; inspect actual angle and record it.
 * Stop immediately if an end stop, stall, abnormal noise or heat occurs.
 */
#define TEST_STEPPER_BUTTON_JOG 1
#define TEST_STEPPER_TARGET_MDEG 100000
#define TEST_STEPPER_TIMEOUT_MS  12000u
#define TEST_SERVO_LEFT_MDEG     30000
#define TEST_SERVO_RIGHT_MDEG    150000
#define TEST_SERVO_CENTER_MDEG   90000
#define TEST_SERVO_HOLD_MS       2000u
#if (TEST_SERVO_RIGHT_MDEG - TEST_SERVO_LEFT_MDEG) != 120000
#error "Servo calibration walk expects a 120 degree commanded span"
#endif
#if TEST_STEPPER_TARGET_MDEG <= 90000
#error "Stepper test must command more than 90 degrees of travel"
#endif
#if TEST_STEPPER && TEST_STEPPER_BUTTON_JOG && !TEST_BUTTON
#error "Stepper button jog requires button enabled"
#endif
#endif
#endif
