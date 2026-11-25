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
 * Default: EVB onboard devices only. Verify external wiring before enabling.
 */
#define TEST_LED          1
#define TEST_ADC          1
#define TEST_BUTTON       1
#define TEST_OLED         0
#define TEST_LCD          0
#define TEST_ULTRASONIC   0
#define TEST_SERVO        0 /* enable only for scope test with servo power disconnected */
#define TEST_STEPPER      0
#endif
#endif
