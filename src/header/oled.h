#ifndef OLED_H_
#define OLED_H_

#include <stdint.h>

/* SSD1306 controller, 128x64, I2C. Not a generic SH1106 driver. */
#define OLED_WIDTH     128
#define OLED_HEIGHT     64
#define OLED_I2C_ADDR 0x3C  /* Change to 0x3D if your module needs it. */

#define OLED_BLACK     0u
#define OLED_WHITE     1u
#define OLED_INVERT    2u  /* DrawPixel only: toggle existing pixel */

/* Hardware initialization: caller must configure clocks first. */
int OLED_Init(void);

/* Drawing functions modify RAM only: call OLED_Update() to show changes. */
void OLED_Clear(void);
void OLED_Fill(uint8_t color);
void OLED_DrawPixel(int16_t x, int16_t y, uint8_t color);
void OLED_DrawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                   uint8_t color);
void OLED_DrawRect(int16_t x, int16_t y, int16_t w, int16_t h,
                   uint8_t color);
void OLED_FillRect(int16_t x, int16_t y, int16_t w, int16_t h,
                   uint8_t color);

/* 1 bpp, row-major, MSB first in each byte; row stride = (width+7)/8.
 * Set opaque=1 to clear 0-bits of the bitmap; opaque=0 leaves them untouched.
 */
void OLED_DrawBitmap(int16_t x, int16_t y, uint16_t width, uint16_t height,
                     const uint8_t *bitmap, uint8_t opaque);

/* 5x7 ASCII font (0x20-0x7e). size=1 => 6x8 per character.
 * Unsupported chars are drawn as '?'. Only set pixels are drawn;
 * clear the background with FillRect before updating dynamic text.
 */
void OLED_DrawChar(int16_t x, int16_t y, char c,
                   uint8_t size, uint8_t color);
void OLED_DrawString(int16_t x, int16_t y, const char *str,
                     uint8_t size, uint8_t color);

/* Legacy synchronous transfer used during initialization and diagnostics. */
int OLED_Update(void);

/* Runtime display path: render in RAM, request a diff pass, then service
 * the LPI2C transfer from main. Dirty 128-byte SSD1306 pages are copied
 * into a stable snapshot; unchanged pages are skipped entirely.
 * No DMA is enabled by this implementation. Each foreground Service call
 * performs bounded work without polling until hardware is ready. */
int OLED_RequestUpdate(void);
void OLED_Service(uint32_t now_ms);
typedef struct {
    uint32_t update_requests;
    uint32_t pages_sent;
    uint32_t pages_skipped;
    uint32_t data_bytes_sent;
    uint32_t errors;
    uint8_t busy;
} OLEDTransferStats;
void OLED_GetTransferStats(OLEDTransferStats *out);
int OLED_SetContrast(uint8_t contrast);
int OLED_SetInverted(uint8_t enabled);

/* Optional framebuffer inspection. Do not change concurrently with Update. */
const uint8_t *OLED_GetBuffer(void);

#endif /* OLED_H_ */
