#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "lcd_hd44780.h"

PCC_Type fake_pcc;
static PORT_Type port;
static GPIO_Type gpio;
static uint32_t delay_calls,delay_total;

static void delay_us(void *context,uint32_t microseconds)
{
    (void)context;
    ++delay_calls;
    delay_total+=microseconds;
}
static LCDPin pin(uint8_t p)
{
    LCDPin result={&port,&gpio,2u,p};
    return result;
}
static LCDHD44780Config config(void)
{
    LCDHD44780Config cfg={0};
    cfg.rs=pin(4u);
    cfg.enable=pin(5u);
    cfg.data[0]=pin(11u);
    cfg.data[1]=pin(12u);
    cfg.data[2]=pin(13u);
    cfg.data[3]=pin(14u);
    cfg.columns=16u;
    cfg.rows=2u;
    cfg.delay_us=delay_us;
    return cfg;
}
static void test_queue_and_latest_wins(void)
{
    LCDHD44780 lcd;
    LCDHD44780Config cfg=config();
    uint32_t baseline;
    assert(LCDHD44780_Init(&lcd,&cfg));
    assert(delay_calls>0u && delay_total>50000u);
    assert(!LCDHD44780_IsBusy(&lcd));
    baseline=delay_calls;
    assert(!LCDHD44780_QueueLine(NULL,0u,"x"));
    assert(!LCDHD44780_QueueLine(&lcd,2u,"x"));
    assert(!LCDHD44780_QueueLine(&lcd,0u,NULL));
    assert(LCDHD44780_QueueLine(&lcd,0u,"FIRST"));
    assert(LCDHD44780_QueueLine(&lcd,1u,"WORLD"));
    assert(LCDHD44780_IsBusy(&lcd));
    assert(lcd.service_steps==0u);
    assert(LCDHD44780_Service(&lcd,0u));
    assert(!LCDHD44780_Service(&lcd,0u)); /* one action per tick */
    assert(lcd.service_steps==1u);
    for(uint32_t ms=1u;ms<450u;ms++) {
        if(ms==5u) {
            assert(LCDHD44780_QueueLine(&lcd,0u,"LATEST"));
            /* Active line snapshot cannot be changed while transmitting. */
            assert(memcmp(lcd.active_line,"FIRST",5u)==0);
        }
        (void)LCDHD44780_Service(&lcd,ms);
        assert(!LCDHD44780_Service(&lcd,ms));
    }
    assert(!LCDHD44780_IsBusy(&lcd));
    assert(memcmp(lcd.displayed[0],"LATEST",6u)==0);
    assert(memcmp(lcd.displayed[1],"WORLD",5u)==0);
    assert(lcd.completed_lines==3u); /* FIRST, LATEST, WORLD */
    assert(lcd.service_steps==3u*4u*17u);
    assert(delay_calls==baseline); /* 0 runtime delay calls */

    /* Unchanged content is free and does not generate more GPIO work. */
    assert(LCDHD44780_QueueLine(&lcd,0u,"LATEST"));
    assert(!LCDHD44780_IsBusy(&lcd));
    assert(!LCDHD44780_Service(&lcd,451u));
    assert(delay_calls==baseline);

    /* Existing independent Bring-up synchronous path stays available. */
    assert(LCDHD44780_WriteLine(&lcd,0u,"DIRECT"));
    assert(delay_calls>baseline);
    assert(memcmp(lcd.displayed[0],"DIRECT",6u)==0);
    assert(!LCDHD44780_IsBusy(&lcd));
}
static void test_tick_wrap(void)
{
    LCDHD44780 lcd;
    LCDHD44780Config cfg=config();
    assert(LCDHD44780_Init(&lcd,&cfg));
    assert(LCDHD44780_QueueLine(&lcd,0u,"WRAP"));
    assert(LCDHD44780_Service(&lcd,UINT32_MAX-1u));
    assert(!LCDHD44780_Service(&lcd,UINT32_MAX-1u));
    assert(LCDHD44780_Service(&lcd,UINT32_MAX));
    assert(!LCDHD44780_Service(&lcd,UINT32_MAX));
    assert(LCDHD44780_Service(&lcd,0u)); /* deadline wraps to zero */
    assert(!LCDHD44780_Service(&lcd,0u));
    for(uint32_t ms=1u;ms<100u;ms++)
        (void)LCDHD44780_Service(&lcd,ms);
    assert(!LCDHD44780_IsBusy(&lcd));
    assert(memcmp(lcd.displayed[0],"WRAP",4u)==0);
}
int main(void)
{
    test_queue_and_latest_wins();
    test_tick_wrap();
    puts("PASS: LCD sync init, queued tick-driven TX, coalescing, wrap, no runtime busy delay");
    return 0;
}
