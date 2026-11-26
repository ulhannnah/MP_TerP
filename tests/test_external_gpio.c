#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "gpio_io.h"
#include "relay_policy.h"
#include "final_io_pins.h"
PCC_Type mock_pcc;
static PORT_Type port_c,port_e;
static GPIO_Type gpio_c,gpio_e;

static void test_buttons(void)
{
    GpioButton aim,relay;
    const GpioButtonConfig a={&port_c,&gpio_c,2u,MP_TERP_FINAL_AIM_PORTC_PIN,true,true};
    const GpioButtonConfig b={&port_c,&gpio_c,2u,MP_TERP_FINAL_RELAY_PORTC_PIN,true,true};
    const uint32_t a_mask=1u<<MP_TERP_FINAL_AIM_PORTC_PIN;
    const uint32_t b_mask=1u<<MP_TERP_FINAL_RELAY_PORTC_PIN;
    assert(MP_TERP_FINAL_AIM_PORTC_PIN==8u &&
           MP_TERP_FINAL_RELAY_PORTC_PIN==9u);
    assert(MP_TERP_PortCDebugPinReserved(4u)); /* SWD_CLK */
    assert(MP_TERP_PortCDebugPinReserved(5u)); /* JTAG_TDI */
    assert(!MP_TERP_PortCDebugPinReserved(8u));
    assert(!MP_TERP_PortCDebugPinReserved(9u));
    gpio_c.PDIR=a_mask|b_mask; /* Pullups: idle HIGH */
    assert(GpioButton_Init(&aim,&a));
    assert(GpioButton_Init(&relay,&b));
    assert((port_c.PCR[MP_TERP_FINAL_AIM_PORTC_PIN]&PORT_PCR_PE_MASK)!=0u);
    assert((port_c.PCR[MP_TERP_FINAL_RELAY_PORTC_PIN]&PORT_PCR_PS_MASK)!=0u);
    assert((port_c.PCR[4u]&(15u<<16))==PORT_PCR_IRQC(10u));
    assert(!GpioButton_TakePressDebounced(&aim,0u,35u));
    gpio_c.PDIR&=~a_mask;
    port_c.ISFR=a_mask;GpioButton_OnPortIrq(&aim);
    port_c.ISFR=0u; /* hardware W1C behavior simulated */
    assert(!GpioButton_TakePressDebounced(&aim,10u,35u));
    assert(!GpioButton_TakePressDebounced(&aim,44u,35u));
    assert(GpioButton_TakePressDebounced(&aim,45u,35u));
    assert(!GpioButton_TakePressDebounced(&aim,100u,35u));
    /* A second falling edge while held must not generate another event. */
    port_c.ISFR=a_mask;GpioButton_OnPortIrq(&aim);
    port_c.ISFR=0u;
    assert(!GpioButton_TakePressDebounced(&aim,101u,35u));
    gpio_c.PDIR|=a_mask;
    assert(!GpioButton_TakePressDebounced(&aim,110u,35u));
    assert(!GpioButton_TakePressDebounced(&aim,145u,35u));
    gpio_c.PDIR&=~a_mask;
    port_c.ISFR=a_mask;GpioButton_OnPortIrq(&aim);
    port_c.ISFR=0u;
    assert(!GpioButton_TakePressDebounced(&aim,150u,35u));
    assert(GpioButton_TakePressDebounced(&aim,185u,35u));
    /* Different input on same PORTC IRQ can be consumed separately. */
    gpio_c.PDIR&=~b_mask;
    port_c.ISFR=b_mask;
    GpioButton_OnPortIrq(&aim); /* ignored */
    GpioButton_OnPortIrq(&relay);
    port_c.ISFR=0u;
    assert(!GpioButton_TakePressDebounced(&relay,200u,35u));
    assert(GpioButton_TakePressDebounced(&relay,235u,35u));
    assert(!GpioButton_TakePressDebounced(&relay,236u,35u));
}
static void test_relay_output(void)
{
    GpioOutput relay;
    const GpioOutputConfig cfg={&port_e,&gpio_e,3u,0u,true};
    const uint32_t mask=1u;
    assert(GpioOutput_Init(&relay,&cfg));
    assert((gpio_e.PCOR&mask)!=0u);
    assert((gpio_e.PDDR&mask)!=0u);
    assert((port_e.PCR[0]&PORT_PCR_MUX(1))!=0u);
    GpioOutput_Write(&relay,true);
    assert((gpio_e.PSOR&mask)!=0u);
    GpioOutput_Write(&relay,false);
    assert((gpio_e.PCOR&mask)!=0u);
}
static void test_relay_interlock(void)
{
    bool armed=false,active=false;
    /* Fake enum values only for the helper: IDLE=0, INDICATING=2. */
    assert(!MP_RelayNewAimCompleted(0u,0u,2u));
    assert(MP_RelayNewAimCompleted(1u,2u,2u));
    assert(!MP_RelayNewAimCompleted(2u,2u,2u));
    armed=true;
    assert(!MP_RelayCanPulse(armed,active,false,false));
    assert(!MP_RelayCanPulse(armed,active,true,true));
    assert(MP_RelayCanPulse(armed,active,true,false));
    active=true;armed=false;
    assert(!MP_RelayCanPulse(armed,active,true,false));
    assert(!MP_RelayPulseExpired(active,1199u,1000u,200u));
    assert(MP_RelayPulseExpired(active,1200u,1000u,200u));
    assert(MP_RelayPulseExpired(true,10u,UINT32_MAX-99u,100u));
    assert(!MP_RelayPulseExpired(false,1200u,1000u,200u));
}
int main(void)
{
    test_buttons();test_relay_output();test_relay_interlock();
    puts("PASS: PTC8/9 IRQ flag buttons, reserved PTC4/5 guarded, relay interlock and pulse expiry");
    return 0;
}
