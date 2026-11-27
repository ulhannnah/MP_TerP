#ifndef MP_TERP_OLED_MOCK_DEVICE_REGISTERS_H
#define MP_TERP_OLED_MOCK_DEVICE_REGISTERS_H
#include <stdint.h>
typedef struct {uint32_t PCR[32];} PORT_Type;
typedef struct {uint32_t PCCn[128];} PCC_Type;
typedef struct {
    uint32_t MCR, MSR, MIER, MDER, MCFGR1, MCCR0, MFCR, MTDR;
} LPI2C_Type;
extern PORT_Type mock_port_a;
extern PCC_Type mock_pcc;
extern LPI2C_Type mock_lpi2c;
#define PORTA (&mock_port_a)
#define PCC (&mock_pcc)
#define LPI2C0 (&mock_lpi2c)
#define PCC_LPI2C0_INDEX 10u
#define PCC_PORTA_INDEX 11u
#define PCC_PCCn_CGC_MASK (1u<<30)
#define PCC_PCCn_PCS_MASK (7u<<24)
#define PCC_PCCn_PCS(x) (((uint32_t)(x)&7u)<<24)
#define PORT_PCR_MUX(x) ((uint32_t)(x)<<8)
#define LPI2C_MSR_NDF_MASK (1u<<10)
#define LPI2C_MSR_ALF_MASK (1u<<11)
#define LPI2C_MSR_FEF_MASK (1u<<12)
#define LPI2C_MSR_PLTF_MASK (1u<<13)
#define LPI2C_MSR_TDF_MASK (1u<<0)
#define LPI2C_MSR_SDF_MASK (1u<<9)
#define LPI2C_MCR_RST_MASK (1u<<1)
#define LPI2C_MCR_MEN_MASK (1u<<0)
#define LPI2C_MCFGR1_PRESCALE(x) ((uint32_t)(x))
#define LPI2C_MCCR0_CLKLO(x) (((uint32_t)(x)&63u)<<0)
#define LPI2C_MCCR0_CLKHI(x) (((uint32_t)(x)&63u)<<8)
#define LPI2C_MCCR0_SETHOLD(x) (((uint32_t)(x)&63u)<<16)
#define LPI2C_MCCR0_DATAVD(x) (((uint32_t)(x)&63u)<<24)
#define LPI2C_MFCR_TXWATER(x) ((uint32_t)(x))
#define LPI2C_MTDR_CMD(x) (((uint32_t)(x)&7u)<<8)
#define LPI2C_MTDR_DATA(x) ((uint32_t)(x)&0xffu)

/* Limited S32K144 eDMA + DMAMUX register mock for opt-in host tests. */
#if defined(MP_TERP_OLED_EDMA_ENABLE) && MP_TERP_OLED_EDMA_ENABLE
typedef struct {
    uint32_t SADDR;
    uint16_t SOFF,ATTR;
    union {uint32_t MLNO;} NBYTES;
    uint32_t SLAST,DADDR;
    uint16_t DOFF;
    union {uint16_t ELINKNO;} CITER;
    uint32_t DLASTSGA;
    uint16_t CSR;
    union {uint16_t ELINKNO;} BITER;
} DMA_TCD_Mock;
typedef struct {
    uint32_t CR;
    uint8_t CERQ,CERR,SERQ;
    uint32_t ERR;
    DMA_TCD_Mock TCD[16];
} DMA_Type;
typedef struct {uint8_t CHCFG[16];} DMAMUX_Type;
extern DMA_Type mock_dma;
extern DMAMUX_Type mock_dmamux;
#define DMA (&mock_dma)
#define DMAMUX (&mock_dmamux)
#define PCC_DMAMUX_INDEX 33u
#define LPI2C_MDER_TDDE_MASK (1u<<0)
#define DMA_CR_EMLM_MASK (1u<<7)
#define DMA_CERQ_CERQ(x) ((uint8_t)((x)&0xfu))
#define DMA_CERR_CERR(x) ((uint8_t)((x)&0xfu))
#define DMA_SERQ_SERQ(x) ((uint8_t)((x)&0xfu))
#define DMA_TCD_SOFF_SOFF(x) ((uint16_t)(x))
#define DMA_TCD_ATTR_SSIZE(x) ((uint16_t)((x)<<8))
#define DMA_TCD_ATTR_DSIZE(x) ((uint16_t)(x))
#define DMA_TCD_NBYTES_MLNO_NBYTES(x) ((uint32_t)(x))
#define DMA_TCD_CITER_ELINKNO_CITER(x) ((uint16_t)(x))
#define DMA_TCD_BITER_ELINKNO_BITER(x) ((uint16_t)(x))
#define DMA_TCD_CSR_DREQ_MASK (1u<<3)
#define DMA_TCD_CSR_DONE_MASK (1u<<7)
#define DMAMUX_CHCFG_SOURCE(x) ((uint8_t)((x)&63u))
#define DMAMUX_CHCFG_ENBL_MASK (1u<<7)
#endif
#endif
