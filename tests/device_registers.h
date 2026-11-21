#ifndef MOCK_DEVICE_REGISTERS_H
#define MOCK_DEVICE_REGISTERS_H
#include <stdint.h>
typedef int IRQn_Type;
enum { LPIT0_Ch0_IRQn=48, LPIT0_Ch1_IRQn=49, LPIT0_Ch2_IRQn=50, LPIT0_Ch3_IRQn=51 };
#define PCC_LPIT_INDEX 55
#define PCC_PCCn_CGC_MASK (1u << 30)
#define PCC_PCCn_PCS_MASK (7u << 24)
#define PCC_PCCn_PCS(x) ((uint32_t)(x) << 24)
#define LPIT_MCR_M_CEN_MASK (1u << 0)
typedef struct {volatile uint32_t TVAL; volatile uint32_t CVAL; volatile uint32_t TCTRL;} MOCK_TMR;
typedef struct {volatile uint32_t MCR; volatile uint32_t MSR; volatile uint32_t MIER; volatile uint32_t SETTEN; volatile uint32_t CLRTEN; MOCK_TMR TMR[4];} MOCK_LPIT;
typedef struct {volatile uint32_t PCCn[120];} MOCK_PCC;
extern MOCK_LPIT mock_lpit;
extern MOCK_PCC mock_pcc;
#define LPIT0 (&mock_lpit)
#define PCC (&mock_pcc)
extern uint32_t mock_primask;
extern uint8_t mock_irq_enabled[52];
void NVIC_EnableIRQ(IRQn_Type irq);
void NVIC_DisableIRQ(IRQn_Type irq);
void NVIC_ClearPendingIRQ(IRQn_Type irq);
uint32_t __get_PRIMASK(void);
void __disable_irq(void);
void __enable_irq(void);
#endif
