#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "oled_port.h"
#include "oled_port_s32k144.h"

/* Compiled -fno-pie -no-pie for test-only 32-bit S32K144 TCD address model. */
PORT_Type mock_port_a;
PCC_Type mock_pcc;
LPI2C_Type mock_lpi2c;
DMA_Type mock_dma;
DMAMUX_Type mock_dmamux;

static void init(void)
{
    OLEDPortConfig cfg={0};
    cfg.bus=LPI2C0;cfg.bus_pcc_index=PCC_LPI2C0_INDEX;
    cfg.bus_clock_pcs=6u;
    cfg.sda_port=PORTA;cfg.sda_port_pcc_index=PCC_PORTA_INDEX;
    cfg.sda_pin=2u;cfg.sda_alt=3u;
    cfg.scl_port=PORTA;cfg.scl_port_pcc_index=PCC_PORTA_INDEX;
    cfg.scl_pin=3u;cfg.scl_alt=3u;
    cfg.prescale=4u;cfg.clock_low=14u;cfg.clock_high=9u;
    cfg.set_hold=8u;cfg.data_valid=4u;
    assert(OLED_PortConfigure(&cfg));
    OLED_PortInit();
    assert((PCC->PCCn[PCC_DMAMUX_INDEX]&PCC_PCCn_CGC_MASK)!=0u);
    assert((LPI2C0->MCR&LPI2C_MCR_MEN_MASK)!=0u);
}
static void test_dma_buffer_and_tcd(void)
{
    const uint8_t data[3]={0x12u,0x34u,0x56u};
    static const uint32_t expected[6]={
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
    assert(DMAMUX->CHCFG[0]==(DMAMUX_CHCFG_SOURCE(45u)|DMAMUX_CHCFG_ENBL_MASK));
    assert((LPI2C0->MDER&LPI2C_MDER_TDDE_MASK)!=0u);
    assert(DMA->TCD[0].SOFF==4u);
    assert(DMA->TCD[0].ATTR==(DMA_TCD_ATTR_SSIZE(2)|DMA_TCD_ATTR_DSIZE(2)));
    assert(DMA->TCD[0].NBYTES.MLNO==4u);
    assert(DMA->TCD[0].CITER.ELINKNO==6u);
    assert(DMA->TCD[0].BITER.ELINKNO==6u);
    assert(DMA->TCD[0].DOFF==0u);
    assert(DMA->TCD[0].DADDR==(uint32_t)(uintptr_t)&LPI2C0->MTDR);
    assert((DMA->TCD[0].CSR&DMA_TCD_CSR_DREQ_MASK)!=0u);
    assert(DMA->TCD[0].SADDR!=0u);
    /* On this non-PIE Linux test, static data pointers fit 32 bits. */
    const uint32_t *src=(const uint32_t *)(uintptr_t)DMA->TCD[0].SADDR;
    for(uint32_t i=0;i<6u;i++)assert(src[i]==expected[i]);
    /* MSR is write-one-to-clear on silicon, plain RW in this mock. */
    LPI2C0->MSR=0u;
    OLED_PortAsyncService();
    assert(OLED_PortAsyncStatus()==1);
    /* DMA major-loop DONE is insufficient without actual I2C STOP. */
    DMA->TCD[0].CSR |= DMA_TCD_CSR_DONE_MASK;
    LPI2C0->MSR=0u;
    OLED_PortAsyncService();
    assert(OLED_PortAsyncStatus()==1);
    LPI2C0->MSR=LPI2C_MSR_SDF_MASK;
    OLED_PortAsyncService();
    assert(OLED_PortAsyncStatus()==0);
    assert(DMAMUX->CHCFG[0]==0u);
    assert((LPI2C0->MDER&LPI2C_MDER_TDDE_MASK)==0u);
}
static void test_error_and_recovery(void)
{
    const uint8_t data[]={0xabu};
    assert(OLED_PortAsyncStart(0x3cu,0x40u,data,1u));
    LPI2C0->MSR=LPI2C_MSR_NDF_MASK;
    OLED_PortAsyncService();
    assert(OLED_PortAsyncStatus()<0);
    assert(DMAMUX->CHCFG[0]==0u);
    OLED_PortAsyncAbort();
    assert(OLED_PortAsyncStatus()<0);
    assert((LPI2C0->MDER&LPI2C_MDER_TDDE_MASK)==0u);
    assert(OLED_PortAsyncStart(0x3cu,0x40u,data,1u));
    DMA->ERR |= 1u;
    LPI2C0->MSR=0u;
    OLED_PortAsyncService();
    assert(OLED_PortAsyncStatus()<0);
    OLED_PortAsyncAbort();
    DMA->ERR=0u; /* emulate peripheral W1C CERR */
}
static void test_edma_minor_loop_mode_is_rejected(void)
{
    DMA->CR=DMA_CR_EMLM_MASK;
    OLED_PortInit();
    const uint8_t data=1u;
    assert(!OLED_PortAsyncStart(0x3cu,0x40u,&data,1u));
    DMA->CR=0u;
    OLED_PortInit();
    assert(OLED_PortAsyncStart(0x3cu,0x40u,&data,1u));
    OLED_PortAsyncAbort();
}
int main(void)
{
    init();
    test_dma_buffer_and_tcd();
    test_error_and_recovery();
    test_edma_minor_loop_mode_is_rejected();
    puts("PASS: S32K144 OLED eDMA TCD/DMAMUX/MTDR encoding, DONE+STOP, fault, EMLM guard");
    return 0;
}
