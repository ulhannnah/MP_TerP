#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "oled.h"
#include "oled_port.h"

/* Mock transport captures the buffer only at completion: this catches
 * edits to a live DMA/FIFO descriptor before STOP. */
static struct {
    uint8_t address, control;
    const uint8_t *source;
    uint16_t length;
    int status;
    uint8_t remaining;
} tx;
static uint32_t n_transactions;
static uint32_t n_data;
static uint32_t n_commands;
static uint8_t last_data[128];
static uint16_t last_data_length;
static uint8_t last_command[6];
static uint32_t data_history_count;
static uint8_t data_history[8][128];
static uint16_t data_length_history[8];
static uint8_t fail_once;

void OLED_PortInit(void){tx.status=0;}
int OLED_PortWrite(uint8_t addr,uint8_t ctrl,const uint8_t *data,uint16_t len)
{
    (void)data;
    if(addr!=OLED_I2C_ADDR || (ctrl!=0u && ctrl!=0x40u) || len>128u)return -1;
    return 0;
}
bool OLED_PortAsyncStart(uint8_t addr,uint8_t ctrl,const uint8_t *data,uint16_t len)
{
    if(tx.status==1 || !data || len==0u || len>128u)return false;
    tx.address=addr;tx.control=ctrl;tx.source=data;tx.length=len;
    tx.status=1;tx.remaining=3u;
    return true;
}
void OLED_PortAsyncService(void)
{
    if(tx.status!=1)return;
    if(tx.remaining!=0u){--tx.remaining;return;}
    if(fail_once){fail_once=0u;tx.status=-2;return;}
    ++n_transactions;
    assert(tx.address==OLED_I2C_ADDR);
    if(tx.control==0u){
        ++n_commands;
        assert(tx.length==6u);
        memcpy(last_command,tx.source,6u);
    }else{
        assert(tx.control==0x40u && tx.length>0u && tx.length<=128u);
        ++n_data;
        last_data_length=tx.length;
        memset(last_data,0,sizeof(last_data));
        memcpy(last_data,tx.source,tx.length);
        if(data_history_count<8u){
            memset(data_history[data_history_count],0,128u);
            memcpy(data_history[data_history_count],tx.source,tx.length);
            data_length_history[data_history_count]=tx.length;
            ++data_history_count;
        }
    }
    tx.status=0;
}
int OLED_PortAsyncStatus(void){return tx.status;}
void OLED_PortAsyncAbort(void){tx.status=-3;}

static void drain(uint32_t start_ms)
{
    OLEDTransferStats s;
    for(uint32_t i=0u;i<1000u;i++){
        OLED_Service(start_ms+i/20u);
        OLED_GetTransferStats(&s);
        if(!s.busy && tx.status!=1)return;
    }
    assert(!"OLED async drain timeout");
}
static void unchanged_page_is_skipped(void)
{
    OLEDTransferStats s;
    assert(OLED_Init()==0);
    assert(OLED_RequestUpdate()==0);
    drain(0u);
    OLED_GetTransferStats(&s);
    assert(s.pages_sent==0u && s.pages_skipped==8u);
    assert(n_transactions==0u);
}
static void single_dirty_page(void)
{
    OLEDTransferStats s;
    OLED_DrawPixel(0,10,OLED_WHITE); /* page 1, column 0, bit 2 */
    assert(OLED_RequestUpdate()==0);
    drain(100u);
    OLED_GetTransferStats(&s);
    assert(s.pages_sent==1u && s.data_bytes_sent==1u);
    assert(n_commands==1u && n_data==1u);
    assert(last_command[0]==0x21u && last_command[1]==0u &&
           last_command[2]==0u && last_command[3]==0x22u &&
           last_command[4]==1u && last_command[5]==1u);
    assert(last_data_length==1u);
    assert(last_data[0]==(1u<<2));
    assert(OLED_RequestUpdate()==0);
    drain(200u);
    assert(n_data==1u); /* nothing changed since last success */
}
static void snapshot_prevents_inflight_corruption(void)
{
    OLEDTransferStats s;
    data_history_count=0u;
    OLED_DrawPixel(1,18,OLED_WHITE); /* page 2 */
    assert(OLED_RequestUpdate()==0);
    OLED_Service(300u); /* queue page address */
    OLED_Service(300u);
    OLED_Service(301u);
    OLED_Service(302u);
    OLED_Service(303u); /* address done: queue page payload snapshot */
    OLED_DrawPixel(2,18,OLED_WHITE); /* update while payload in flight */
    assert(OLED_RequestUpdate()==0);
    drain(304u);
    OLED_GetTransferStats(&s);
    assert(data_history_count==2u);
    assert(data_length_history[0]==1u && data_history[0][0]==0x04u);
    assert(data_length_history[1]==1u && data_history[1][0]==0x04u);
    assert(last_command[1]==2u && last_command[2]==2u);
    assert(s.pages_sent==3u); /* previous page1 + page2 twice */
}
/* Two separated changed columns use one bounded contiguous window.
 * No unchanged page is retransmitted; unchanged columns inside the window
 * are included because the SSD1306 command supports a contiguous span. */
static void dirty_column_window(void)
{
    OLEDTransferStats before,after;
    OLED_GetTransferStats(&before);
    OLED_DrawPixel(10,33,OLED_WHITE); /* page 4, column 10 */
    OLED_DrawPixel(20,33,OLED_WHITE); /* page 4, column 20 */
    assert(OLED_RequestUpdate()==0);
    drain(450u);
    OLED_GetTransferStats(&after);
    assert(after.pages_sent==before.pages_sent+1u);
    assert(after.data_bytes_sent==before.data_bytes_sent+11u);
    assert(last_command[0]==0x21u && last_command[1]==10u &&
           last_command[2]==20u && last_command[3]==0x22u &&
           last_command[4]==4u && last_command[5]==4u);
    assert(last_data_length==11u);
    assert(last_data[0]==0x02u && last_data[10]==0x02u);
    for(uint8_t i=1u;i<10u;i++)assert(last_data[i]==0u);
}
static void transient_failure_does_not_commit_shadow(void)
{
    OLEDTransferStats s;
    OLED_DrawPixel(5,26,OLED_WHITE); /* page 3 */
    assert(OLED_RequestUpdate()==0);
    fail_once=1u;
    for(uint32_t i=0u;i<30u;i++)OLED_Service(500u+i);
    OLED_GetTransferStats(&s);
    assert(s.errors>=1u);
    /* The failed page is not marked as displayed; retry after backoff. */
    drain(900u);
    OLED_GetTransferStats(&s);
    assert(s.pages_sent>=5u);
}
int main(void)
{
    unchanged_page_is_skipped();
    single_dirty_page();
    snapshot_prevents_inflight_corruption();
    dirty_column_window();
    transient_failure_does_not_commit_shadow();
    puts("PASS: SSD1306 async dirty page/columns, immutable snapshot, retry, unchanged skip");
    return 0;
}
