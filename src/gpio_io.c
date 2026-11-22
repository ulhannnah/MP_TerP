#include "gpio_io.h"
#include <stddef.h>

/* Minimal Cortex-M PRIMASK critical section. S32K SDK variants do not
 * necessarily expose CMSIS __get_PRIMASK/__disable_irq/__enable_irq.
 * Restore the exact previous interrupt masking state on exit.
 */
static uint32_t gpio_enter_critical(void)
{
#if defined(__arm__) || defined(__thumb__)
    uint32_t primask;
    __asm volatile ("mrs %0, primask\n\tcpsid i"
                    : "=r"(primask) :: "memory");
    return primask;
#else
    return 0u;
#endif
}

static void gpio_exit_critical(uint32_t primask)
{
#if defined(__arm__) || defined(__thumb__)
    __asm volatile ("msr primask, %0" :: "r"(primask) : "memory");
#else
    (void)primask;
#endif
}

static bool valid_pin(PORT_Type *p,GPIO_Type *g,uint8_t pin){
    return p!=NULL&&g!=NULL&&pin<32u;
}
bool GpioOutput_Init(GpioOutput *out,const GpioOutputConfig *cfg){
    uint32_t mask;
    if(!out||!cfg||!valid_pin(cfg->port,cfg->gpio,cfg->pin))return false;
    out->config=*cfg;out->initialized=0u;
    mask=(uint32_t)1u<<cfg->pin;
    PCC->PCCn[cfg->port_pcc_index]|=PCC_PCCn_CGC_MASK;
    cfg->port->PCR[cfg->pin]=PORT_PCR_MUX(1);
    cfg->gpio->PDDR|=mask;
    if(cfg->active_high)cfg->gpio->PCOR=mask;
    else cfg->gpio->PSOR=mask;
    out->initialized=1u;return true;
}
void GpioOutput_Write(GpioOutput *out,bool active){
    uint32_t mask;
    if(!out||!out->initialized)return;
    mask=(uint32_t)1u<<out->config.pin;
    if(active==out->config.active_high)out->config.gpio->PSOR=mask;
    else out->config.gpio->PCOR=mask;
}
bool GpioButton_Init(GpioButton *btn,const GpioButtonConfig *cfg){
    uint32_t pcr;
    if(!btn||!cfg||!valid_pin(cfg->port,cfg->gpio,cfg->pin))return false;
    btn->config=*cfg;btn->pending=0u;btn->initialized=0u;
    PCC->PCCn[cfg->port_pcc_index]|=PCC_PCCn_CGC_MASK;
    pcr=PORT_PCR_MUX(1);
    if(cfg->internal_pullup)pcr|=PORT_PCR_PE_MASK|PORT_PCR_PS_MASK;
    /* IRQC=10 falling edge, IRQC=9 rising edge. */
    pcr|=PORT_PCR_IRQC(cfg->active_low?10u:9u);
    cfg->gpio->PDDR &= ~((uint32_t)1u<<cfg->pin);
    cfg->port->ISFR=(uint32_t)1u<<cfg->pin;
    cfg->port->PCR[cfg->pin]=pcr;
    btn->initialized=1u;return true;
}
void GpioButton_OnPortIrq(GpioButton *btn){
    uint32_t mask;
    if(!btn||!btn->initialized)return;
    mask=(uint32_t)1u<<btn->config.pin;
    if((btn->config.port->ISFR&mask)!=0u){
        btn->config.port->ISFR=mask;
        btn->pending=1u;
    }
}
bool GpioButton_TakePress(GpioButton *btn){
    bool pressed;
    if(!btn||!btn->initialized)return false;
    uint32_t primask=gpio_enter_critical();
    pressed=(btn->pending!=0u);btn->pending=0u;
    gpio_exit_critical(primask);
    return pressed;
}
