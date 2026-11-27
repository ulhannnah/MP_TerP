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
#endif
