#ifndef MP_TERP_FINAL_IO_PINS_H
#define MP_TERP_FINAL_IO_PINS_H

#include <stdbool.h>
#include <stdint.h>

/* S32K144EVB-Q100 external button allocation, 2026-11-26 correction.
 * PTC4 is JTAG_TCLK/SWD_CLK; PTC5 is JTAG_TDI. NEVER switch either to GPIO:
 * doing so can prevent P&E OpenSDA from connecting to the target MCU.
 *
 * Per NXP EVB header mapping:
 * PTC8 = J6-09 (D19), external Active-Low AIM
 * PTC9 = J6-11 (D18), external Active-Low RELAY command
 * PTE0 = J3-06, active-high relay control signal only
 * PTB0 = J4-05, external analog potentiometer (ADC0_SE4)
 */
#define MP_TERP_FINAL_AIM_PORTC_PIN 8u
#define MP_TERP_FINAL_RELAY_PORTC_PIN 9u
#define MP_TERP_FINAL_RELAY_PORTE_PIN 0u
#define MP_TERP_FINAL_POT_PORTB_PIN 0u

static inline bool MP_TERP_PortCDebugPinReserved(uint8_t pin)
{
    return pin == 4u || pin == 5u;
}

#endif /* MP_TERP_FINAL_IO_PINS_H */
