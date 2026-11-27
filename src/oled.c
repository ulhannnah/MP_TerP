#include "oled.h"
#include "oled_port.h"
#include "oled_font5x7.h"
#include <string.h>

#define OLED_BUFFER_BYTES ((OLED_WIDTH * OLED_HEIGHT) / 8)
#define OLED_TX_CHUNK     128u
#define OLED_PAGE_COUNT   8u
#define OLED_ASYNC_TIMEOUT_MS  200u
#define OLED_ASYNC_RETRY_MS    250u

static uint8_t oled_framebuffer[OLED_BUFFER_BYTES];

/* Only the foreground accesses these state variables. 128-byte snapshots
 * cannot be overwritten until the port reports that STOP has completed. */
static uint8_t s_displayed[OLED_BUFFER_BYTES];
static uint8_t s_page_data[OLED_TX_CHUNK];
static uint8_t s_page_commands[6];
static uint8_t s_page_valid[OLED_PAGE_COUNT];
static uint8_t s_refresh_requested, s_rescan_requested;
static uint8_t s_scan_page, s_active_page, s_async_phase, s_initialized;
static uint32_t s_phase_started_ms, s_retry_after_ms;
static OLEDTransferStats s_stats;


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

    /* Synchronous boot setup is intentionally preserved. All subsequent
     * Stage5 refreshes use the non-blocking path, not OLED_Update(). */
    s_initialized=0u;
    s_refresh_requested=0u;s_rescan_requested=0u;
    s_scan_page=0u;s_active_page=0u;s_async_phase=0u;
    s_retry_after_ms=0u;
    memset(&s_stats,0,sizeof(s_stats));
    memset(s_page_valid,0,sizeof(s_page_valid));
    OLED_PortInit();
    for (i = 0u; i < sizeof(commands); ++i)
    {
        r = OLED_Command(commands[i]);
        if (r != 0) return r;
    }
    OLED_Clear();
    r=OLED_Update();
    if(r==0)s_initialized=1u;
    return r;
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
    if(s_async_phase!=0u)return -6; /* unsafe to mix sync/async on bus */
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
    memcpy(s_displayed,oled_framebuffer,sizeof(s_displayed));
    for(uint8_t page=0u;page<OLED_PAGE_COUNT;page++)s_page_valid[page]=1u;
    return 0;
}

int OLED_RequestUpdate(void)
{
    if(!s_initialized)return -1;
    ++s_stats.update_requests;
    if(s_refresh_requested || s_async_phase!=0u)
        s_rescan_requested=1u;
    else {
        s_refresh_requested=1u;
        s_scan_page=0u;
    }
    return 0;
}

/* A single call does one bounded piece of work: either start a transaction,
 * feed one LPI2C TX FIFO command, or inspect at most eight page digests.
 * This prevents SSD1306 transport from delaying the foreground scanner. */
void OLED_Service(uint32_t now_ms)
{
    int result;
    if(!s_initialized)return;
    if((int32_t)(now_ms-s_retry_after_ms)<0)return;
    if(s_async_phase!=0u){
        OLED_PortAsyncService();
        result=OLED_PortAsyncStatus();
        if(result==1){
            if((uint32_t)(now_ms-s_phase_started_ms)>
               OLED_ASYNC_TIMEOUT_MS) {
                OLED_PortAsyncAbort();
                ++s_stats.errors;
                s_async_phase=0u;
                s_retry_after_ms=now_ms+OLED_ASYNC_RETRY_MS;
            }
            return;
        }
        if(result<0){
            OLED_PortAsyncAbort(); /* clear stranded FIFO / recover bus */
            ++s_stats.errors;
            s_async_phase=0u;
            s_retry_after_ms=now_ms+OLED_ASYNC_RETRY_MS;
            return;
        }
        if(s_async_phase==1u){
            if(!OLED_PortAsyncStart(OLED_I2C_ADDR,0x40u,
                                    s_page_data,OLED_TX_CHUNK)){
                ++s_stats.errors;
                s_async_phase=0u;
                s_retry_after_ms=now_ms+OLED_ASYNC_RETRY_MS;
                return;
            }
            s_phase_started_ms=now_ms;
            s_async_phase=2u;
        } else {
            memcpy(&s_displayed[(uint16_t)s_active_page*OLED_TX_CHUNK],
                   s_page_data,OLED_TX_CHUNK);
            s_page_valid[s_active_page]=1u;
            ++s_stats.pages_sent;
            s_stats.data_bytes_sent+=OLED_TX_CHUNK;
            s_scan_page=(uint8_t)(s_active_page+1u);
            s_async_phase=0u;
        }
        return;
    }
    if(!s_refresh_requested)return;
    while(s_scan_page<OLED_PAGE_COUNT){
        const uint16_t offset=(uint16_t)s_scan_page*OLED_TX_CHUNK;
        if(!s_page_valid[s_scan_page] ||
           memcmp(&s_displayed[offset],&oled_framebuffer[offset],
                  OLED_TX_CHUNK)!=0)break;
        ++s_stats.pages_skipped;
        ++s_scan_page;
    }
    if(s_scan_page==OLED_PAGE_COUNT){
        s_refresh_requested=s_rescan_requested;
        s_rescan_requested=0u;
        s_scan_page=0u;
        return;
    }
    s_active_page=s_scan_page;
    memcpy(s_page_data,
           &oled_framebuffer[(uint16_t)s_active_page*OLED_TX_CHUNK],
           OLED_TX_CHUNK);
    /* A single command transaction constrains both column and page.
     * Data transaction then writes exactly one page (128 bytes). */
    s_page_commands[0]=0x21u; s_page_commands[1]=0u;
    s_page_commands[2]=127u;  s_page_commands[3]=0x22u;
    s_page_commands[4]=s_active_page;
    s_page_commands[5]=s_active_page;
    if(!OLED_PortAsyncStart(OLED_I2C_ADDR,0x00u,s_page_commands,6u)){
        ++s_stats.errors;
        s_retry_after_ms=now_ms+OLED_ASYNC_RETRY_MS;
        return;
    }
    s_phase_started_ms=now_ms;
    s_async_phase=1u;
}
void OLED_GetTransferStats(OLEDTransferStats *out)
{
    if(!out)return;
    *out=s_stats;
    out->busy=(uint8_t)(s_async_phase!=0u || s_refresh_requested!=0u);
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
