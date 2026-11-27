#include "oled_port.h"
#include "oled_port_s32k144.h"
#include "device_registers.h"
#include <stddef.h>

#define PORT_I2C_ERRORS (LPI2C_MSR_NDF_MASK | LPI2C_MSR_ALF_MASK | \
                         LPI2C_MSR_FEF_MASK | LPI2C_MSR_PLTF_MASK)
#define PORT_WAIT_LIMIT (1000000u)

/* At most one MTDR write per foreground service; never spin on TDF. */
typedef struct {
    const uint8_t *data;
    uint16_t len, sent;
    uint8_t address, control, phase;
    uint8_t active;
    int status;
} OLEDAsyncPort;
static OLEDAsyncPort s_async;

/* Existing wiring preserved as fallback; all parameters may be overridden. */
static OLEDPortConfig s_config;
static uint8_t s_has_config;
static LPI2C_Type *s_bus;

#if MP_TERP_OLED_EDMA_ENABLE
/* Experimental S32K144 LPI2C0 TX eDMA path, channel 0 exclusively.
 * TCD: 32-bit SRAM command/data word => 32-bit MTDR, one word per
 * LPI2C TX request. DMA DONE means FIFO supplied, not STOP observed.
 * The foreground must still verify LPI2C SDF before committing a page. */
#define OLED_EDMA_CH     0u
#define OLED_EDMA_SOURCE 45u
#define OLED_EDMA_WORDS  (128u + 3u) /* START + control + data + STOP */
static uint32_t s_dma_words[OLED_EDMA_WORDS];
static uint8_t s_edma_ready;
static uint8_t s_edma_active;
static void oled_edma_disable(void)
{
    DMAMUX->CHCFG[OLED_EDMA_CH]=0u;
    DMA->CERQ=DMA_CERQ_CERQ(OLED_EDMA_CH);
    if(s_bus)s_bus->MDER &= ~LPI2C_MDER_TDDE_MASK;
    s_edma_active=0u;
}
static bool oled_edma_init(void)
{
    /* Do not interfere with unrelated DMA clients / minor loop mode. */
    if(s_bus!=LPI2C0 || (DMA->CR&DMA_CR_EMLM_MASK)!=0u)
        return false;
    PCC->PCCn[PCC_DMAMUX_INDEX] |= PCC_PCCn_CGC_MASK;
    oled_edma_disable();
    DMA->CERR=DMA_CERR_CERR(OLED_EDMA_CH);
    return true;
}
static bool oled_edma_start(uint8_t address7,uint8_t control,
                            const uint8_t *data,uint16_t len)
{
    const uint16_t count=(uint16_t)(len+3u);
    if(!s_edma_ready || s_edma_active || count>OLED_EDMA_WORDS)
        return false;
    s_dma_words[0]=LPI2C_MTDR_CMD(4)|
                   LPI2C_MTDR_DATA((uint32_t)address7<<1);
    s_dma_words[1]=LPI2C_MTDR_CMD(0)|LPI2C_MTDR_DATA(control);
    for(uint16_t i=0u;i<len;i++)
        s_dma_words[(uint16_t)(i+2u)]=LPI2C_MTDR_CMD(0)|
                                     LPI2C_MTDR_DATA(data[i]);
    s_dma_words[(uint16_t)(len+2u)]=LPI2C_MTDR_CMD(2);
    oled_edma_disable();
    DMA->CERR=DMA_CERR_CERR(OLED_EDMA_CH);
    /* Program the TCD while hardware requests are disabled. */
    DMA->TCD[OLED_EDMA_CH].CSR=0u;
    DMA->TCD[OLED_EDMA_CH].SADDR=(uint32_t)(uintptr_t)&s_dma_words[0];
    DMA->TCD[OLED_EDMA_CH].SOFF=DMA_TCD_SOFF_SOFF(4);
    DMA->TCD[OLED_EDMA_CH].ATTR=DMA_TCD_ATTR_SSIZE(2)|
                                 DMA_TCD_ATTR_DSIZE(2);
    DMA->TCD[OLED_EDMA_CH].NBYTES.MLNO=DMA_TCD_NBYTES_MLNO_NBYTES(4);
    DMA->TCD[OLED_EDMA_CH].SLAST=0u;
    DMA->TCD[OLED_EDMA_CH].DADDR=(uint32_t)(uintptr_t)&s_bus->MTDR;
    DMA->TCD[OLED_EDMA_CH].DOFF=0u;
    DMA->TCD[OLED_EDMA_CH].CITER.ELINKNO=
        DMA_TCD_CITER_ELINKNO_CITER(count);
    DMA->TCD[OLED_EDMA_CH].DLASTSGA=0u;
    DMA->TCD[OLED_EDMA_CH].BITER.ELINKNO=
        DMA_TCD_BITER_ELINKNO_BITER(count);
    /* DREQ drops channel request when the major loop completes.
     * No DMA interrupt: the existing bounded service polls DONE+SDF. */
    DMA->TCD[OLED_EDMA_CH].CSR=DMA_TCD_CSR_DREQ_MASK;
    DMAMUX->CHCFG[OLED_EDMA_CH]=
        DMAMUX_CHCFG_SOURCE(OLED_EDMA_SOURCE)|DMAMUX_CHCFG_ENBL_MASK;
    s_edma_active=1u;
    s_bus->MDER |= LPI2C_MDER_TDDE_MASK;
    DMA->SERQ=DMA_SERQ_SERQ(OLED_EDMA_CH);
    return true;
}
#endif

bool OLED_PortConfigure(const OLEDPortConfig *cfg){
    if(!cfg||!cfg->bus||!cfg->sda_port||!cfg->scl_port||
       cfg->sda_pin>=32u||cfg->scl_pin>=32u||
       cfg->sda_alt<2u||cfg->sda_alt>7u||
       cfg->scl_alt<2u||cfg->scl_alt>7u||
       cfg->bus_clock_pcs==0u||cfg->bus_clock_pcs>7u||
       cfg->prescale>7u||cfg->clock_low==0u||cfg->clock_high==0u)
        return false;
    s_config=*cfg;s_has_config=1u;return true;
}
void OLED_PortInit(void){
    OLEDPortConfig cfg;
    if(s_has_config) cfg=s_config;
    else {
        cfg.bus=LPI2C0;cfg.bus_pcc_index=PCC_LPI2C0_INDEX;cfg.bus_clock_pcs=6u;
        cfg.sda_port=PORTA;cfg.sda_port_pcc_index=PCC_PORTA_INDEX;
        cfg.sda_pin=2u;cfg.sda_alt=3u;
        cfg.scl_port=PORTA;cfg.scl_port_pcc_index=PCC_PORTA_INDEX;
        cfg.scl_pin=3u;cfg.scl_alt=3u;
        cfg.prescale=4u;cfg.clock_low=14u;cfg.clock_high=9u;
        cfg.set_hold=8u;cfg.data_valid=4u;
    }
    s_bus=cfg.bus;
    s_async.active=0u;
    s_async.status=0;
#if MP_TERP_OLED_EDMA_ENABLE
    s_edma_ready=0u;
    s_edma_active=0u;
#endif
    PCC->PCCn[cfg.sda_port_pcc_index] |= PCC_PCCn_CGC_MASK;
    PCC->PCCn[cfg.scl_port_pcc_index] |= PCC_PCCn_CGC_MASK;
    cfg.sda_port->PCR[cfg.sda_pin]=PORT_PCR_MUX(cfg.sda_alt);
    cfg.scl_port->PCR[cfg.scl_pin]=PORT_PCR_MUX(cfg.scl_alt);
    PCC->PCCn[cfg.bus_pcc_index]&=~PCC_PCCn_CGC_MASK;
    PCC->PCCn[cfg.bus_pcc_index]=(PCC->PCCn[cfg.bus_pcc_index]&~PCC_PCCn_PCS_MASK)|
                                  PCC_PCCn_PCS(cfg.bus_clock_pcs)|PCC_PCCn_CGC_MASK;
    s_bus->MCR=LPI2C_MCR_RST_MASK;
    s_bus->MCR=0u;
    s_bus->MCFGR1=LPI2C_MCFGR1_PRESCALE(cfg.prescale);
    s_bus->MCCR0=LPI2C_MCCR0_CLKLO(cfg.clock_low) |
                  LPI2C_MCCR0_CLKHI(cfg.clock_high) |
                  LPI2C_MCCR0_SETHOLD(cfg.set_hold) |
                  LPI2C_MCCR0_DATAVD(cfg.data_valid);
    s_bus->MFCR=LPI2C_MFCR_TXWATER(0);
    s_bus->MCR=LPI2C_MCR_MEN_MASK;
#if MP_TERP_OLED_EDMA_ENABLE
    s_edma_ready=(uint8_t)(oled_edma_init()?1u:0u);
#endif
}
static int port_put(uint16_t value){
    if(!s_bus)return -5;
    for(uint32_t limit=0u;limit<PORT_WAIT_LIMIT;limit++){
        uint32_t msr=s_bus->MSR;
        if((msr&PORT_I2C_ERRORS)!=0u)return -2;
        if((msr&LPI2C_MSR_TDF_MASK)!=0u){
            s_bus->MTDR=value;return 0;
        }
    }
    return -1;
}
int OLED_PortWrite(uint8_t address7,uint8_t control,
                   const uint8_t *data,uint16_t len){
    int result;
    if(!s_bus)return -5;
    if(s_async.active)return -6;
    if((data==0&&len!=0u)||address7>0x7fu)return -4;
    s_bus->MSR=LPI2C_MSR_SDF_MASK|PORT_I2C_ERRORS;
    result=port_put(LPI2C_MTDR_CMD(4)|LPI2C_MTDR_DATA((uint32_t)address7<<1));
    if(result)return result;
    result=port_put(LPI2C_MTDR_CMD(0)|LPI2C_MTDR_DATA(control));
    if(result)return result;
    for(uint16_t i=0u;i<len;i++){
        result=port_put(LPI2C_MTDR_CMD(0)|LPI2C_MTDR_DATA(data[i]));
        if(result)return result;
    }
    result=port_put(LPI2C_MTDR_CMD(2));
    if(result)return result;
    for(uint32_t limit=0u;limit<PORT_WAIT_LIMIT;limit++){
        uint32_t msr=s_bus->MSR;
        if((msr&PORT_I2C_ERRORS)!=0u)return -2;
        if((msr&LPI2C_MSR_SDF_MASK)!=0u){
            s_bus->MSR=LPI2C_MSR_SDF_MASK;return 0;
        }
    }
    return -3;
}

/* Non-blocking foreground port service. Only one FIFO word can be sent
 * per call, so it does not monopolize the app's cooperative event loop. */
bool OLED_PortAsyncStart(uint8_t address7, uint8_t control,
                         const uint8_t *data, uint16_t len)
{
    if(!s_bus || s_async.active || (data==NULL && len!=0u) ||
       address7>0x7fu)return false;
    s_async.data=data;
    s_async.len=len;
    s_async.sent=0u;
    s_async.address=address7;
    s_async.control=control;
    s_async.phase=0u;
    s_bus->MSR=LPI2C_MSR_SDF_MASK|PORT_I2C_ERRORS;
#if MP_TERP_OLED_EDMA_ENABLE
    /* In eDMA mode never fall back halfway through a DMA transaction.
     * A failed start is retried through the upper-layer 250ms recovery. */
    if(!oled_edma_start(address7,control,data,len))return false;
#endif
    s_async.status=1;
    s_async.active=1u;
    return true;
}

void OLED_PortAsyncService(void)
{
    uint32_t msr;
    if(!s_async.active || !s_bus)return;
    msr=s_bus->MSR;
    if((msr&PORT_I2C_ERRORS)!=0u){
#if MP_TERP_OLED_EDMA_ENABLE
        if(s_edma_active)oled_edma_disable();
#endif
        s_async.active=0u;s_async.status=-2;return;
    }
#if MP_TERP_OLED_EDMA_ENABLE
    if((DMA->ERR&(1u<<OLED_EDMA_CH))!=0u){
        oled_edma_disable();
        s_async.active=0u;s_async.status=-5;return;
    }
    /* A transfer is only complete after both TCD major loop completion
     * AND the STOP flag. DONE alone means data may still be in the FIFO. */
    if((DMA->TCD[OLED_EDMA_CH].CSR&DMA_TCD_CSR_DONE_MASK)!=0u &&
       (msr&LPI2C_MSR_SDF_MASK)!=0u){
        oled_edma_disable();
        s_bus->MSR=LPI2C_MSR_SDF_MASK;
        s_async.active=0u;s_async.status=0;
    }
    return;
#else
    if(s_async.phase==4u){
        if((msr&LPI2C_MSR_SDF_MASK)!=0u){
            s_bus->MSR=LPI2C_MSR_SDF_MASK;
            s_async.active=0u;s_async.status=0;
        }
        return;
    }
    if((msr&LPI2C_MSR_TDF_MASK)==0u)return;
    switch(s_async.phase){
    case 0u:
        s_bus->MTDR=LPI2C_MTDR_CMD(4)|
                    LPI2C_MTDR_DATA((uint32_t)s_async.address<<1);
        s_async.phase=1u;
        break;
    case 1u:
        s_bus->MTDR=LPI2C_MTDR_CMD(0)|
                    LPI2C_MTDR_DATA(s_async.control);
        s_async.phase=2u;
        break;
    case 2u:
        if(s_async.sent<s_async.len){
            s_bus->MTDR=LPI2C_MTDR_CMD(0)|
                        LPI2C_MTDR_DATA(s_async.data[s_async.sent]);
            ++s_async.sent;
        } else {
            s_bus->MTDR=LPI2C_MTDR_CMD(2);
            s_async.phase=4u;
        }
        break;
    default:
        s_async.active=0u;s_async.status=-4;
        break;
    }
#endif
}
int OLED_PortAsyncStatus(void){return s_async.status;}
void OLED_PortAsyncAbort(void)
{
    /* Reinitialize on either a bus fault OR a timeout. A failed async write
     * may have commands stranded in the TX FIFO, so retrying without reset
     * is not safe. This executes at most once per bounded retry interval. */
    if(!s_bus)return;
#if MP_TERP_OLED_EDMA_ENABLE
    if(s_edma_active)oled_edma_disable();
#endif
    OLED_PortInit();
    s_async.active=0u;
    s_async.status=-3;
}
