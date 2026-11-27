#ifndef OLED_PORT_H_
#define OLED_PORT_H_

#include <stdint.h>
#include <stdbool.h>

/* Port layer: change only these functions when moving to another MCU/bus. */
void OLED_PortInit(void);
/* 7-bit I2C address, SSD1306 control byte (0x00 command / 0x40 data).
 * Return 0 on success or a negative error code on failure.
 */
int OLED_PortWrite(uint8_t address7, uint8_t control,
                   const uint8_t *data, uint16_t len);

/* Runtime non-blocking LPI2C transfer. Each service call advances at most
 * one FIFO command and never waits for a peripheral flag. The data buffer
 * must remain unchanged until status transitions from 1 (busy) to 0.
 * Status: 1=pending, 0=complete/idle, negative=error.
 * The legacy blocking OLED_PortWrite remains for boot initialization only. */
bool OLED_PortAsyncStart(uint8_t address7, uint8_t control,
                         const uint8_t *data, uint16_t len);
void OLED_PortAsyncService(void);
int OLED_PortAsyncStatus(void);
void OLED_PortAsyncAbort(void);
/* Transport selected by this build and initialized at runtime.
 * 0 = bounded CPU LPI2C FIFO service, 1 = opt-in eDMA engine ready.
 * eDMA mode uses the same async API (no DMA IRQ registration required). */
uint8_t OLED_PortTransportMode(void);

#endif /* OLED_PORT_H_ */
