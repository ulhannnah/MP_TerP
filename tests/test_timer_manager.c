#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "device_registers.h"
#include "timer_manager.h"
MOCK_LPIT mock_lpit;
MOCK_PCC mock_pcc;
uint32_t mock_primask;
uint8_t mock_irq_enabled[52];
static uint32_t nvic_pending_clear[52];
void NVIC_EnableIRQ(IRQn_Type irq){assert(irq>=48 && irq<=51); mock_irq_enabled[irq]=1u;}
void NVIC_DisableIRQ(IRQn_Type irq){assert(irq>=48 && irq<=51); mock_irq_enabled[irq]=0u;}
void NVIC_ClearPendingIRQ(IRQn_Type irq){assert(irq>=48 && irq<=51); ++nvic_pending_clear[irq];}
uint32_t __get_PRIMASK(void){return mock_primask;}
void __disable_irq(void){mock_primask=1u;}
void __enable_irq(void){mock_primask=0u;}
void LPIT0_Ch0_IRQHandler(void);
void LPIT0_Ch1_IRQHandler(void);
void LPIT0_Ch2_IRQHandler(void);
void LPIT0_Ch3_IRQHandler(void);
static uint32_t calls[2];
static void counter(void *ctx){uint32_t *n=(uint32_t*)ctx; ++(*n);}
static void trigger(uint32_t ch){mock_lpit.MSR = (1u<<ch); switch(ch){case 0:LPIT0_Ch0_IRQHandler();break;case 1:LPIT0_Ch1_IRQHandler();break;case 2:LPIT0_Ch2_IRQHandler();break;default:LPIT0_Ch3_IRQHandler();break;}}
int main(void){
  assert(TimerManager_Start(TIMER_CH_STEPPER)==TIMER_ERR_NOT_INITIALIZED);
  assert(TimerManager_Init()==TIMER_OK);
  assert((mock_pcc.PCCn[PCC_LPIT_INDEX]&PCC_PCCn_CGC_MASK)!=0);
  assert((mock_pcc.PCCn[PCC_LPIT_INDEX]&PCC_PCCn_PCS(6))==PCC_PCCn_PCS(6));
  assert(mock_lpit.TMR[2].TVAL==39999u);
  assert((mock_lpit.TMR[2].TCTRL&1u)==1u);
  assert(mock_irq_enabled[50]==1u);
  assert(TimerManager_Start(TIMER_CH_STEPPER)==TIMER_ERR_NOT_CONFIGURED);
  assert(TimerManager_Configure(TIMER_CH_SYSTEM,1000,false)==TIMER_ERR_RESERVED);
  assert(TimerManager_Configure(TIMER_CH_RESERVED,1000,false)==TIMER_ERR_RESERVED);
  assert(TimerManager_Configure((TimerChannel)-1,1000,false)==TIMER_ERR_ARGUMENT);
  assert(TimerManager_Configure(TIMER_CH_TRIGGER,0,true)==TIMER_ERR_RANGE);
  assert(TimerManager_Configure(TIMER_CH_TRIGGER,107374183u,true)==TIMER_ERR_RANGE);
  assert(TimerManager_Configure(TIMER_CH_TRIGGER,10u,true)==TIMER_OK);
  assert(mock_lpit.TMR[1].TVAL==399u);
  assert((mock_lpit.TMR[1].TCTRL&(1u<<17))!=0u);
  assert((mock_lpit.TMR[1].TCTRL&(1u<<16))==0u);
  assert(TimerManager_RegisterCallback(TIMER_CH_TRIGGER,counter,&calls[1])==TIMER_OK);
  assert(TimerManager_Start(TIMER_CH_TRIGGER)==TIMER_OK);
  assert(TimerManager_RegisterCallback(TIMER_CH_TRIGGER,counter,&calls[1])==TIMER_ERR_BUSY);
  trigger(1u); assert(calls[1]==1u);
  /* In real hardware TSOI stops timer at timeout; emulate T_EN falling. */
  mock_lpit.TMR[1].TCTRL &= ~1u;
  assert(TimerManager_Start(TIMER_CH_TRIGGER)==TIMER_OK);
  assert((mock_lpit.TMR[1].TCTRL&1u)!=0u);
  assert(TimerManager_Stop(TIMER_CH_TRIGGER)==TIMER_OK);
  assert((mock_lpit.TMR[1].TCTRL&1u)==0u);
  assert(TimerManager_Configure(TIMER_CH_STEPPER,2500u,false)==TIMER_OK);
  assert(mock_lpit.TMR[0].TVAL==99999u);
  assert((mock_lpit.TMR[0].TCTRL&(1u<<17))==0u);
  assert(TimerManager_RegisterCallback(TIMER_CH_STEPPER,counter,&calls[0])==TIMER_OK);
  assert(TimerManager_Start(TIMER_CH_STEPPER)==TIMER_OK);
  trigger(0u);trigger(0u);assert(calls[0]==2u);
  assert(TimerManager_TakeOledRequest()==false);
  assert(TimerManager_TakeLcdRequest()==false);
  for (int i=1;i<=500;i++) {
    trigger(2u);
    assert(TimerManager_GetTickMs()==(uint32_t)i);
    if(i%100==0){assert(TimerManager_TakeOledRequest());assert(!TimerManager_TakeOledRequest());}
    if(i%200==0){assert(TimerManager_TakeLcdRequest());assert(!TimerManager_TakeLcdRequest());}
  }
  mock_primask=1u;
  trigger(2u);
  assert(TimerManager_TakeOledRequest()==false);
  assert(mock_primask==1u);
  assert(TimerManager_Init()==TIMER_OK);
  assert(TimerManager_GetTickMs()==0u);
  assert(TimerManager_Start(TIMER_CH_STEPPER)==TIMER_ERR_NOT_CONFIGURED);
  puts("PASS: LPIT configuration, one-shot TSOI, callback, start/stop, display cadence, re-init");
  return 0;
}
