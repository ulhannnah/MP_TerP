#ifndef GPIO_MOCK_DEVICE_REGISTERS_H
#define GPIO_MOCK_DEVICE_REGISTERS_H
#include <stdint.h>
typedef struct { volatile uint32_t PCR[32]; volatile uint32_t ISFR; } PORT_Type;
typedef struct {
    volatile uint32_t PSOR,PCOR,PDDR,PDIR;
} GPIO_Type;
typedef struct { volatile uint32_t PCCn[128]; } PCC_Type;
extern PCC_Type mock_pcc;
#define PCC (&mock_pcc)
#define PCC_PCCn_CGC_MASK (1u<<30)
#define PORT_PCR_MUX(x) ((uint32_t)(x)<<8)
#define PORT_PCR_IRQC(x) ((uint32_t)(x)<<16)
#define PORT_PCR_PE_MASK (1u<<1)
#define PORT_PCR_PS_MASK (1u<<0)
#endif
