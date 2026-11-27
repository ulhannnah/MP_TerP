/*
 * Deterministic, hardware-free SSD1306 traffic simulation.
 * Compiles the PRODUCTION src/oled.c (dirty page/column algorithm), not a
 * copied optimization formula. A page-addressed panel mock validates every
 * byte of the resulting GDDRAM after each completed frame.
 *
 * The "legacy" wire count uses the verified src/oled.c from
 * final/radar-system-20261126: 6 single-byte commands plus 8 128-byte
 * data transactions per frame. I2C includes slave address/control bytes.
 * No attempt is made to simulate ARM instruction timings, eDMA bandwidth,
 * bus rise times, IRQ latency, or power draw.
 */
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "oled.h"
#include "oled_port.h"

#define PANEL_BYTES 1024u
#define LEGACY_WIRE_BYTES ((6u*3u)+(8u*(128u+2u)))
static uint8_t panel[PANEL_BYTES];
static struct {
    const uint8_t *data;
    uint16_t len;
    uint8_t control;
    uint8_t active;
    int status;
} tx;
static uint8_t active_page, first_column, last_column, have_window;
static uint64_t wire_bytes, tx_count, cmd_count, data_count;
static uint32_t service_ms;

void OLED_PortInit(void) { tx.active=0u; tx.status=0; }
int OLED_PortWrite(uint8_t addr,uint8_t control,const uint8_t *data,uint16_t len)
{
    /* The legacy synchronous boot path clears the panel: no traffic is
     * included in the runtime comparison. */
    assert(addr==OLED_I2C_ADDR);
    assert(control==0u || control==0x40u);
    assert(data!=NULL && len>0u && len<=128u);
    return 0;
}
bool OLED_PortAsyncStart(uint8_t addr,uint8_t control,const uint8_t *data,uint16_t len)
{
    if(addr!=OLED_I2C_ADDR || tx.active || !data || !len || len>128u ||
       (control!=0u && control!=0x40u))return false;
    tx.data=data; tx.len=len; tx.control=control;
    tx.active=1u;tx.status=1;
    return true;
}
void OLED_PortAsyncService(void)
{
    if(!tx.active)return;
    assert(tx.status==1);
    ++tx_count;
    wire_bytes+=(uint64_t)tx.len+2u; /* address + SSD1306 control */
    if(tx.control==0u) {
        assert(tx.len==6u);
        assert(tx.data[0]==0x21u && tx.data[3]==0x22u);
        first_column=tx.data[1];
        last_column=tx.data[2];
        assert(first_column<=last_column && last_column<128u);
        assert(tx.data[4]==tx.data[5] && tx.data[4]<8u);
        active_page=tx.data[4];
        have_window=1u;
        ++cmd_count;
    } else {
        assert(have_window);
        assert(tx.len==(uint16_t)(last_column-first_column+1u));
        memcpy(&panel[(uint16_t)active_page*128u+first_column],
               tx.data,tx.len);
        have_window=0u;
        ++data_count;
    }
    tx.active=0u;tx.status=0;
}
int OLED_PortAsyncStatus(void){return tx.status;}
void OLED_PortAsyncAbort(void){assert(!"unexpected OLED abort in traffic test");}

static void flush_and_check(void)
{
    OLEDTransferStats stats;
    assert(OLED_RequestUpdate()==0);
    for(unsigned steps=0u;steps<1000u;steps++){
        OLED_Service(service_ms);
        ++service_ms;
        OLED_GetTransferStats(&stats);
        if(!stats.busy && !tx.active){
            assert(memcmp(OLED_GetBuffer(),panel,PANEL_BYTES)==0);
            return;
        }
    }
    assert(!"OLED simulator failed to drain pending transactions");
}

static void report(const char *name,uint32_t frames,
                   uint64_t old_wire_start,OLEDTransferStats previous)
{
    OLEDTransferStats s;
    OLED_GetTransferStats(&s);
    const uint64_t wire_delta=wire_bytes-old_wire_start;
    const uint64_t old_wire=(uint64_t)frames*LEGACY_WIRE_BYTES;
    const uint32_t data_delta=s.data_bytes_sent-previous.data_bytes_sent;
    const uint32_t pages_delta=s.pages_sent-previous.pages_sent;
    assert(s.errors==0u);
    assert(wire_delta<=UINT32_MAX);
    const double reduction=100.0*(1.0-(double)wire_delta/(double)old_wire);
    /* I2C 100kbps, nominal 9 clocks per byte including ACK. Wire overhead
     * is modeled; inter-transaction idle and analog effects are not. */
    printf("%-17s %4u %9llu %9llu %8u %7u %8.2f%% %8.2fms %8.2fms\\n",
           name,frames,
           (unsigned long long)old_wire,
           (unsigned long long)wire_delta,
           data_delta,pages_delta,reduction,
           old_wire*9.0/100000.0*1000.0,
           wire_delta*9.0/100000.0*1000.0);
}

static OLEDTransferStats snapshot(void)
{
    OLEDTransferStats s;
    OLED_GetTransferStats(&s);
    return s;
}

static void static_screen(void)
{
    const uint32_t frames=40u;
    OLEDTransferStats old=snapshot();
    uint64_t start=wire_bytes;
    for(uint32_t i=0;i<frames;i++){
        OLED_Clear();
        OLED_DrawString(0,0,"FIXED SENSOR",1u,OLED_WHITE);
        OLED_DrawString(0,16,"SERVO SWEEP ONLY",1u,OLED_WHITE);
        OLED_DrawString(0,32,"NO 2D TARGETS",1u,OLED_WHITE);
        OLED_DrawString(0,48,"MANUAL 90 DEG",1u,OLED_WHITE);
        flush_and_check();
    }
    report("static diagnostic",frames,start,old);
}

static void changing_text(void)
{
    const uint32_t frames=60u;
    OLEDTransferStats old=snapshot();
    uint64_t start=wire_bytes;
    for(uint32_t i=0;i<frames;i++){
        char buf[32];
        OLED_Clear();
        OLED_DrawString(0,0,"FIXED SENSOR",1u,OLED_WHITE);
        OLED_DrawString(0,16,"SERVO SWEEP ONLY",1u,OLED_WHITE);
        OLED_DrawString(0,32,"NO 2D TARGETS",1u,OLED_WHITE);
        snprintf(buf,sizeof(buf),"MANUAL %lu DEG",
                 (unsigned long)(30u+(i*7u)%121u));
        OLED_DrawString(0,48,buf,1u,OLED_WHITE);
        flush_and_check();
    }
    report("moving text",frames,start,old);
}

static void scanning_line(void)
{
    const uint32_t frames=120u;
    OLEDTransferStats old=snapshot();
    uint64_t start=wire_bytes;
    for(uint32_t i=0;i<frames;i++){
        /* Stylized synthetic scan, not an exact call into RadarRenderer. */
        const int16_t scan_x=(int16_t)((i*9u)%128u);
        OLED_Clear();
        OLED_DrawString(0,0,"RADAR DEMO",1u,OLED_WHITE);
        OLED_DrawLine(64,63,0,10,OLED_WHITE);
        OLED_DrawLine(64,63,127,10,OLED_WHITE);
        OLED_DrawLine(64,63,scan_x,12,OLED_WHITE);
        OLED_DrawPixel((int16_t)((i*3u)%128u),24,OLED_WHITE);
        flush_and_check();
    }
    report("synthetic scan",frames,start,old);
}

static void full_inversion(void)
{
    const uint32_t frames=40u;
    OLEDTransferStats old=snapshot();
    uint64_t start=wire_bytes;
    for(uint32_t i=0;i<frames;i++){
        OLED_Fill((i&1u)?OLED_WHITE:OLED_BLACK);
        flush_and_check();
    }
    report("full-frame flip",frames,start,old);
}

int main(void)
{
    assert(OLED_Init()==0);
    memset(panel,0,sizeof(panel));
    wire_bytes=0u;tx_count=0u;cmd_count=0u;data_count=0u;
    puts("Scenario           Frames LegacyBytes OptBytes DataBytes Pages  Reduction  LegacyBus    OptBus");
    static_screen();
    changing_text();
    scanning_line();
    full_inversion();
    assert(tx_count==cmd_count+data_count);
    assert(cmd_count==data_count);
    printf("PASS: production OLED driver + SSD1306 mock GDDRAM byte exact, %llu transactions\\n",
           (unsigned long long)tx_count);
    return 0;
}
