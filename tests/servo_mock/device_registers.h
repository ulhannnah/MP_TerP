#ifndef SERVO_MOCK_REGISTERS_H
#define SERVO_MOCK_REGISTERS_H
#include <stdint.h>
#define PCC_PORTA_INDEX 20u
#define PCC_FTM0_INDEX 40u
#define PCC_FTM1_INDEX 41u
#define PCC_FTM2_INDEX 42u
#define PCC_FTM3_INDEX 43u
#define PCC_PCCn_CGC_MASK (1u << 30)
#define PCC_PCCn_PCS_MASK (7u << 24)
#define PCC_PCCn_PCS(x) ((uint32_t)(x) << 24)
#define PORT_PCR_MUX(x) ((uint32_t)(x) << 8)
#define FTM_MODE_WPDIS_MASK (1u << 2)
#define FTM_CnSC_MSB_MASK (1u << 5)
#define FTM_CnSC_ELSB_MASK (1u << 3)
#define FTM_SC_PS(x) ((uint32_t)(x) << 0)
#define FTM_SC_CLKS(x) ((uint32_t)(x) << 3)
#define FTM_SC_PWMEN0_MASK (1u << 16)
typedef struct {volatile uint32_t CnSC,CnV;} FTM_CHANNEL_Type;
typedef struct {
    volatile uint32_t SC,CNT,CNTIN,MOD,MODE,COMBINE,POL;
    FTM_CHANNEL_Type CONTROLS[8];
} FTM_Type;
typedef struct {volatile uint32_t PCR[32];} PORT_Type;
typedef struct {volatile uint32_t PCCn[128];} PCC_Type;
extern FTM_Type fake_ftms[4];
extern PORT_Type fake_port;
extern PCC_Type fake_pcc;
#define FTM0 (&fake_ftms[0])
#define FTM1 (&fake_ftms[1])
#define FTM2 (&fake_ftms[2])
#define FTM3 (&fake_ftms[3])
#define PCC (&fake_pcc)
#endif
