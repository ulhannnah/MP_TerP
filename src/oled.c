#include "oled.h"
#include "oled_port.h"
#include "oled_font5x7.h"
#include <string.h>

#define OLED_BUFFER_BYTES ((OLED_WIDTH * OLED_HEIGHT) / 8)
#define OLED_TX_CHUNK     128u

static uint8_t oled_framebuffer[OLED_BUFFER_BYTES];

static int OLED_Command(uint8_t value)
{
    return OLED_PortWrite(OLED_I2C_ADDR, 0x00u, &value, 1u);
}

int OLED_Init(void)
{
    /* Configuration for SSD1306 128x64 OLED with internal charge pump.
     * Tested prototype sequence, now isolated behind transport layer.
     */
    static const uint8_t commands[] = {
        0xAE,             /* Display off */
        0xD5, 0x80,       /* Clock divide */
        0xA8, 0x3F,       /* Multiplex = 64 */
        0xD3, 0x00,       /* Offset = 0 */
        0x40,             /* Start line = 0 */
        0x8D, 0x14,       /* Charge pump on */
        0x20, 0x00,       /* Horizontal address mode */
        0xA1,             /* Segment remap */
        0xC8,             /* COM scan direction */
        0xDA, 0x12,       /* COM pins for 128x64 */
        0x81, 0x7F,       /* Contrast */
        0xD9, 0xF1,       /* Pre-charge */
        0xDB, 0x40,       /* VCOMH */
        0xA4,             /* Display GDDRAM */
        0xA6,             /* Normal display */
        0x2E,             /* Stop scrolling */
        0xAF              /* Display on */
    };
    unsigned i;
    int r;

    OLED_PortInit();
    for (i = 0u; i < sizeof(commands); ++i)
    {
        r = OLED_Command(commands[i]);
        if (r != 0) return r;
    }
    OLED_Clear();
    return OLED_Update();
}

void OLED_Clear(void)
{
    memset(oled_framebuffer, 0, sizeof(oled_framebuffer));
}

void OLED_Fill(uint8_t color)
{
    memset(oled_framebuffer, color ? 0xFF : 0x00,
           sizeof(oled_framebuffer));
}

void OLED_DrawPixel(int16_t x, int16_t y, uint8_t color)
{
    uint16_t pos;
    uint8_t mask;
    if (x < 0 || y < 0 || x >= OLED_WIDTH || y >= OLED_HEIGHT)
        return;

    pos = (uint16_t)x + ((uint16_t)y >> 3) * OLED_WIDTH;
    mask = (uint8_t)(1u << ((uint16_t)y & 7u));

    if (color == OLED_INVERT)
        oled_framebuffer[pos] ^= mask;
    else if (color == OLED_WHITE)
        oled_framebuffer[pos] |= mask;
    else
        oled_framebuffer[pos] &= (uint8_t)~mask;
}

void OLED_DrawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                   uint8_t color)
{
    int dx = x0 < x1 ? x1 - x0 : x0 - x1;
    int dy = y0 < y1 ? y1 - y0 : y0 - y1;
    int sx = x0 < x1 ? 1 : -1;
    int sy = y0 < y1 ? 1 : -1;
    int err = dx - dy;

    for (;;)
    {
        int e2;
        OLED_DrawPixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x0 = (int16_t)(x0 + sx); }
        if (e2 <  dx) { err += dx; y0 = (int16_t)(y0 + sy); }
    }
}

void OLED_DrawRect(int16_t x, int16_t y, int16_t w, int16_t h,
                   uint8_t color)
{
    if (w <= 0 || h <= 0) return;
    OLED_DrawLine(x, y, (int16_t)(x + w - 1), y, color);
    OLED_DrawLine(x, (int16_t)(y + h - 1),
                  (int16_t)(x + w - 1), (int16_t)(y + h - 1), color);
    OLED_DrawLine(x, y, x, (int16_t)(y + h - 1), color);
    OLED_DrawLine((int16_t)(x + w - 1), y,
                  (int16_t)(x + w - 1), (int16_t)(y + h - 1), color);
}

void OLED_FillRect(int16_t x, int16_t y, int16_t w, int16_t h,
                   uint8_t color)
{
    int xx, yy;
    if (w <= 0 || h <= 0) return;
    /* Clip before drawing to avoid costly loops for off-screen shapes. */
    for (yy = (y < 0 ? 0 : y);
         yy < (int)y + h && yy < OLED_HEIGHT; ++yy)
        for (xx = (x < 0 ? 0 : x);
             xx < (int)x + w && xx < OLED_WIDTH; ++xx)
            OLED_DrawPixel((int16_t)xx, (int16_t)yy, color);
}

void OLED_DrawBitmap(int16_t x, int16_t y, uint16_t width, uint16_t height,
                     const uint8_t *bitmap, uint8_t opaque)
{
    uint16_t row, col;
    uint32_t stride;
    if (bitmap == 0 || width == 0u || height == 0u) return;
    stride = ((uint32_t)width + 7u) / 8u;

    for (row = 0u; row < height; ++row)
    {
        for (col = 0u; col < width; ++col)
        {
            uint8_t bit = (uint8_t)(0x80u >> (col & 7u));
            uint8_t on = (bitmap[(uint32_t)row * stride + (col >> 3)] & bit) != 0u;
            if (on)
                OLED_DrawPixel((int16_t)(x + col), (int16_t)(y + row), OLED_WHITE);
            else if (opaque)
                OLED_DrawPixel((int16_t)(x + col), (int16_t)(y + row), OLED_BLACK);
        }
    }
}

void OLED_DrawChar(int16_t x, int16_t y, char c,
                   uint8_t size, uint8_t color)
{
    uint8_t col, row;
    const uint8_t *glyph;
    if (size == 0u || size > 8u) return;
    if ((unsigned char)c < 0x20u || (unsigned char)c > 0x7Eu)
        c = '?';
    glyph = OLED_Font5x7[(uint8_t)c - 0x20u];

    for (col = 0u; col < 5u; ++col)
        for (row = 0u; row < 7u; ++row)
            if ((glyph[col] & (1u << row)) != 0u)
                OLED_FillRect((int16_t)(x + col * size),
                              (int16_t)(y + row * size),
                              size, size, color);
}

void OLED_DrawString(int16_t x, int16_t y, const char *str,
                     uint8_t size, uint8_t color)
{
    int16_t cx = x;
    if (str == 0 || size == 0u || size > 8u) return;
    while (*str != '\0')
    {
        if (*str == '\n')
        {
            cx = x;
            y = (int16_t)(y + (int16_t)8 * size);
        }
        else
        {
            OLED_DrawChar(cx, y, *str, size, color);
            cx = (int16_t)(cx + (int16_t)6 * size);
        }
        ++str;
    }
}

int OLED_Update(void)
{
    uint16_t offset;
    int r;
    r = OLED_Command(0x21u); if (r) return r;
    r = OLED_Command(0u);    if (r) return r;
    r = OLED_Command(127u);  if (r) return r;
    r = OLED_Command(0x22u); if (r) return r;
    r = OLED_Command(0u);    if (r) return r;
    r = OLED_Command(7u);    if (r) return r;

    /* The SSD1306 memory address auto-increments in horizontal mode.
     * Each successive I2C data transaction continues at that address.
     */
    for (offset = 0u; offset < OLED_BUFFER_BYTES; offset += OLED_TX_CHUNK)
    {
        r = OLED_PortWrite(OLED_I2C_ADDR, 0x40u,
                           &oled_framebuffer[offset], OLED_TX_CHUNK);
        if (r != 0) return r;
    }
    return 0;
}

int OLED_SetContrast(uint8_t contrast)
{
    int r = OLED_Command(0x81u);
    return r ? r : OLED_Command(contrast);
}

int OLED_SetInverted(uint8_t enabled)
{
    return OLED_Command(enabled ? 0xA7u : 0xA6u);
}

const uint8_t *OLED_GetBuffer(void)
{
    return oled_framebuffer;
}
