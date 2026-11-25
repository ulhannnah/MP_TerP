#include "lcd_hd44780.h"
#include <stddef.h>
#include <string.h>

static bool pin_valid(const LCDPin *p){
    return p && p->port && p->gpio && p->pin<32u;
}
static void pin_write(const LCDPin *p,bool high){
    uint32_t mask=(uint32_t)1u<<p->pin;
    if(high)p->gpio->PSOR=mask;
    else p->gpio->PCOR=mask;
}
static void pin_init(const LCDPin *p){
    uint32_t mask=(uint32_t)1u<<p->pin;
    PCC->PCCn[p->port_pcc_index]|=PCC_PCCn_CGC_MASK;
    p->gpio->PCOR=mask;
    p->port->PCR[p->pin]=PORT_PCR_MUX(1);
    p->gpio->PDDR|=mask;
}
static void pause_us(LCDHD44780 *lcd,uint32_t us){
    lcd->config.delay_us(lcd->config.delay_context,us);
}
static void output_nibble(LCDHD44780 *lcd,uint8_t value){
    for(uint8_t bit=0;bit<4u;bit++)
        pin_write(&lcd->config.data[bit],(value & ((uint8_t)1u<<bit))!=0u);
}
static void send_nibble(LCDHD44780 *lcd,uint8_t value){
    output_nibble(lcd,value);
    pin_write(&lcd->config.enable,true);
    pause_us(lcd,2u);
    pin_write(&lcd->config.enable,false);
    pause_us(lcd,50u);
}
static void send_byte(LCDHD44780 *lcd,uint8_t value,bool rs){
    pin_write(&lcd->config.rs,rs);
    send_nibble(lcd,(uint8_t)(value>>4));
    send_nibble(lcd,(uint8_t)(value&15u));
}
static void command(LCDHD44780 *lcd,uint8_t value){
    send_byte(lcd,value,false);
    if(value==0x01u||value==0x02u)pause_us(lcd,2000u);
}
/* Bounded formatting: never reads beyond a source NUL. */
static void format_line(char out[LCDHD44780_MAX_COLS+1u],
                        const char *src,uint8_t columns)
{
    uint8_t col=0u;
    while(col<columns && src[col]!='\0'){
        out[col]=src[col];
        ++col;
    }
    while(col<columns)out[col++]=' ';
    out[columns]='\0';
}
bool LCDHD44780_Init(LCDHD44780 *lcd,const LCDHD44780Config *cfg){
    if(!lcd||!cfg||!cfg->delay_us||cfg->columns==0u||
       cfg->columns>LCDHD44780_MAX_COLS||
       cfg->rows==0u||cfg->rows>2u||!pin_valid(&cfg->rs)||
       !pin_valid(&cfg->enable))
        return false;
    for(unsigned i=0;i<4u;i++)if(!pin_valid(&cfg->data[i]))return false;
    lcd->initialized=0u;lcd->config=*cfg;
    lcd->tx_active=0u;lcd->service_started=0u;
    lcd->active_row=0u;lcd->byte_index=0u;lcd->phase=0u;
    lcd->next_action_ms=0u;lcd->service_steps=0u;lcd->completed_lines=0u;
    for(uint8_t row=0u;row<2u;row++){
        format_line(lcd->desired[row],"",cfg->columns);
        format_line(lcd->displayed[row],"",cfg->columns);
        format_line(lcd->active_line,"",cfg->columns);
    }

    pin_init(&cfg->rs);pin_init(&cfg->enable);
    for(unsigned i=0;i<4u;i++)pin_init(&cfg->data[i]);
    pause_us(lcd,50000u);
    pin_write(&cfg->rs,false);
    send_nibble(lcd,3u);pause_us(lcd,5000u);
    send_nibble(lcd,3u);pause_us(lcd,200u);
    send_nibble(lcd,3u);pause_us(lcd,200u);
    send_nibble(lcd,2u);
    command(lcd,cfg->rows==1u?0x20u:0x28u);
    command(lcd,0x0Cu);
    command(lcd,0x06u);
    command(lcd,0x01u);
    lcd->initialized=1u;return true;
}
bool LCDHD44780_WriteLine(void *context,uint8_t row,const char *text){
    LCDHD44780 *lcd=(LCDHD44780 *)context;
    if(!lcd||!lcd->initialized||!text||row>=lcd->config.rows||
       lcd->tx_active)return false;
    command(lcd,(uint8_t)(0x80u+(row==0u?0u:0x40u)));
    char formatted[LCDHD44780_MAX_COLS+1u];
    format_line(formatted,text,lcd->config.columns);
    for(uint8_t col=0u;col<lcd->config.columns;col++)
        send_byte(lcd,(uint8_t)formatted[col],true);
    (void)memcpy(lcd->displayed[row],formatted,lcd->config.columns+1u);
    (void)memcpy(lcd->desired[row],formatted,lcd->config.columns+1u);
    return true;
}
void LCDHD44780_Clear(LCDHD44780 *lcd){
    if(!lcd||!lcd->initialized||lcd->tx_active)return;
    command(lcd,0x01u);
    for(uint8_t row=0u;row<lcd->config.rows;row++){
        format_line(lcd->desired[row],"",lcd->config.columns);
        format_line(lcd->displayed[row],"",lcd->config.columns);
    }
}
bool LCDHD44780_QueueLine(void *context,uint8_t row,const char *text){
    LCDHD44780 *lcd=(LCDHD44780 *)context;
    if(!lcd||!lcd->initialized||!text||row>=lcd->config.rows)return false;
    format_line(lcd->desired[row],text,lcd->config.columns);
    return true;
}
bool LCDHD44780_IsBusy(const LCDHD44780 *lcd){
    if(!lcd||!lcd->initialized)return false;
    if(lcd->tx_active)return true;
    for(uint8_t row=0u;row<lcd->config.rows;row++){
        if(memcmp(lcd->desired[row],lcd->displayed[row],
                  lcd->config.columns)!=0)return true;
    }
    return false;
}
bool LCDHD44780_Service(LCDHD44780 *lcd,uint32_t now_ms){
    uint8_t data=0u;
    bool rs;
    if(!lcd||!lcd->initialized)return false;

    /* Time gate also applies between completed lines, and works at tick wrap. */
    if(lcd->service_started &&
       (int32_t)(now_ms-lcd->next_action_ms)<0)return false;

    if(!lcd->tx_active){
        uint8_t row;
        for(row=0u;row<lcd->config.rows;row++){
            if(memcmp(lcd->desired[row],lcd->displayed[row],
                      lcd->config.columns)!=0)break;
        }
        if(row==lcd->config.rows)return false;
        lcd->active_row=row;
        lcd->byte_index=0u;
        lcd->phase=0u;
        (void)memcpy(lcd->active_line,lcd->desired[row],
                     lcd->config.columns+1u);
        lcd->tx_active=1u;
    }
    rs=lcd->byte_index!=0u;
    if(rs)data=(uint8_t)lcd->active_line[lcd->byte_index-1u];
    else data=(uint8_t)(0x80u+(lcd->active_row==0u?0u:0x40u));

    /* One edge per call. Data and RS are stable through the following
     * 1ms E high phase; a complete byte spans >=4ms. This avoids all
     * runtime busy waits and does not allocate another timer channel. */
    if(lcd->phase==0u||lcd->phase==2u){
        pin_write(&lcd->config.rs,rs);
        output_nibble(lcd,lcd->phase==0u?(uint8_t)(data>>4):
                                            (uint8_t)(data&15u));
        pin_write(&lcd->config.enable,true);
    }else{
        pin_write(&lcd->config.enable,false);
    }
    lcd->phase=(uint8_t)((lcd->phase+1u)&3u);
    if(lcd->phase==0u){
        ++lcd->byte_index;
        if(lcd->byte_index>lcd->config.columns){
            (void)memcpy(lcd->displayed[lcd->active_row],lcd->active_line,
                         lcd->config.columns+1u);
            lcd->tx_active=0u;
            ++lcd->completed_lines;
        }
    }
    lcd->service_started=1u;
    lcd->next_action_ms=now_ms+1u;
    ++lcd->service_steps;
    return true;
}
