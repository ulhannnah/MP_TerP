#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "oled_port.h"
#include "oled_port_s32k144.h"
PORT_Type mock_port_a;
PCC_Type mock_pcc;
LPI2C_Type mock_lpi2c;

static void init_port(void)
{
    OLEDPortConfig cfg={0};
    cfg.bus=LPI2C0;
    cfg.bus_pcc_index=PCC_LPI2C0_INDEX;
    cfg.bus_clock_pcs=6u;
    cfg.sda_port=PORTA;cfg.sda_port_pcc_index=PCC_PORTA_INDEX;
    cfg.sda_pin=2u;cfg.sda_alt=3u;
    cfg.scl_port=PORTA;cfg.scl_port_pcc_index=PCC_PORTA_INDEX;
    cfg.scl_pin=3u;cfg.scl_alt=3u;
    cfg.prescale=4u;cfg.clock_low=14u;cfg.clock_high=9u;
    cfg.set_hold=8u;cfg.data_valid=4u;
    assert(OLED_PortConfigure(&cfg));
    OLED_PortInit();
    assert(OLED_PortTransportMode()==0u);
    assert((mock_lpi2c.MCR&LPI2C_MCR_MEN_MASK)!=0u);
}
static void test_one_word_per_service_and_stop(void)
{
    const uint8_t data[3]={0x12u,0x34u,0x56u};
    const uint32_t expected[6]={
        LPI2C_MTDR_CMD(4)|LPI2C_MTDR_DATA(0x78u),
        LPI2C_MTDR_CMD(0)|LPI2C_MTDR_DATA(0x40u),
        LPI2C_MTDR_CMD(0)|LPI2C_MTDR_DATA(0x12u),
        LPI2C_MTDR_CMD(0)|LPI2C_MTDR_DATA(0x34u),
        LPI2C_MTDR_CMD(0)|LPI2C_MTDR_DATA(0x56u),
        LPI2C_MTDR_CMD(2)
    };
    assert(OLED_PortAsyncStart(0x3cu,0x40u,data,3u));
    assert(!OLED_PortAsyncStart(0x3cu,0x40u,data,3u));
    assert(OLED_PortAsyncStatus()==1);
    mock_lpi2c.MSR=0u;
    OLED_PortAsyncService();
    assert(mock_lpi2c.MTDR==0u); /* no TDF: return immediately */
    for(uint32_t i=0u;i<6u;i++){
        mock_lpi2c.MSR=LPI2C_MSR_TDF_MASK;
        OLED_PortAsyncService();
        assert(mock_lpi2c.MTDR==expected[i]);
    }
    assert(OLED_PortAsyncStatus()==1); /* STOP must complete */
    mock_lpi2c.MSR=0u;
    OLED_PortAsyncService();
    assert(OLED_PortAsyncStatus()==1);
    mock_lpi2c.MSR=LPI2C_MSR_SDF_MASK;
    OLED_PortAsyncService();
    assert(OLED_PortAsyncStatus()==0);
}
static void test_error_abort_recovery(void)
{
    const uint8_t data[1]={0xabu};
    assert(OLED_PortAsyncStart(0x3cu,0x40u,data,1u));
    mock_lpi2c.MSR=LPI2C_MSR_NDF_MASK;
    OLED_PortAsyncService();
    assert(OLED_PortAsyncStatus()<0);
    OLED_PortAsyncAbort();
    assert(OLED_PortAsyncStatus()<0);
    assert((mock_lpi2c.MCR&LPI2C_MCR_MEN_MASK)!=0u);
    assert(OLED_PortAsyncStart(0x3cu,0x40u,data,1u));
}
int main(void)
{
    init_port();
    test_one_word_per_service_and_stop();
    test_error_abort_recovery();
    puts("PASS: LPI2C bounded async FIFO TX, command sequence, STOP, error recovery");
    return 0;
}
