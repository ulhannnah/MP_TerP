#ifndef MP_TERP_LCD_MOCK_REGISTERS_H
#define MP_TERP_LCD_MOCK_REGISTERS_H
#include <stdint.h>
#define PCC_PCCn_CGC_MASK (1u << 30)
#define PORT_PCR_MUX(v) ((uint32_t)(v)<<8)
typedef struct {
    volatile uint32_t PCR[32];
} PORT_Type;
typedef struct {
    volatile uint32_t PSOR,PCOR,PDDR;
} GPIO_Type;
typedef struct {
    volatile uint32_t PCCn[128];
} PCC_Type;
extern PCC_Type fake_pcc;
#define PCC (&fake_pcc)
#endif
