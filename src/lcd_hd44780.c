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
static void send_nibble(LCDHD44780 *lcd,uint8_t value){
    for(uint8_t bit=0;bit<4u;bit++)
        pin_write(&lcd->config.data[bit],(value & ((uint8_t)1u<<bit))!=0u);
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
bool LCDHD44780_Init(LCDHD44780 *lcd,const LCDHD44780Config *cfg){
    if(!lcd||!cfg||!cfg->delay_us||cfg->columns==0u||cfg->columns>20u||
       cfg->rows==0u||cfg->rows>2u||!pin_valid(&cfg->rs)||!pin_valid(&cfg->enable))
        return false;
    for(unsigned i=0;i<4u;i++)if(!pin_valid(&cfg->data[i]))return false;
    lcd->initialized=0u;lcd->config=*cfg;
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
    if(!lcd||!lcd->initialized||!text||row>=lcd->config.rows)return false;
    command(lcd,(uint8_t)(0x80u+(row==0u?0u:0x40u)));
    size_t n=strlen(text);
    for(uint8_t col=0u;col<lcd->config.columns;col++)
        send_byte(lcd,col<n?(uint8_t)text[col]:(uint8_t)' ',true);
    return true;
}
void LCDHD44780_Clear(LCDHD44780 *lcd){
    if(lcd&&lcd->initialized)command(lcd,0x01u);
}
