#ifndef OLED_PORT_S32K144_H
#define OLED_PORT_S32K144_H
#include <stdbool.h>
#include <stdint.h>
#include "device_registers.h"
/* Optional override for existing OLED_PortInit/OLED_PortWrite. Configure
 * before OLED_Init(), otherwise the original LPI2C0 PTA2/PTA3 is used. */
typedef struct {
    LPI2C_Type *bus;
    uint32_t bus_pcc_index;
    uint8_t bus_clock_pcs;
    PORT_Type *sda_port;
    uint32_t sda_port_pcc_index;
    uint8_t sda_pin;
    uint8_t sda_alt;
    PORT_Type *scl_port;
    uint32_t scl_port_pcc_index;
    uint8_t scl_pin;
    uint8_t scl_alt;
    uint8_t prescale;
    uint8_t clock_low;
    uint8_t clock_high;
    uint8_t set_hold;
    uint8_t data_valid;
} OLEDPortConfig;
bool OLED_PortConfigure(const OLEDPortConfig *config);
#endif
