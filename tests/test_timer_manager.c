#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "device_registers.h"
#include "timer_manager.h"

MOCK_LPIT mock_lpit;
MOCK_PCC mock_pcc;
uint32_t mock_primask;
uint8_t mock_irq_enabled[52];
static uint32_t pending_clear[52], callback_counts[4];

void NVIC_EnableIRQ(IRQn_Type irq){assert(irq>=48 && irq<=51);mock_irq_enabled[irq]=1u;}
void NVIC_DisableIRQ(IRQn_Type irq){assert(irq>=48 && irq<=51);mock_irq_enabled[irq]=0u;}
void NVIC_ClearPendingIRQ(IRQn_Type irq){assert(irq>=48 && irq<=51);++pending_clear[irq];}
uint32_t __get_PRIMASK(void){return mock_primask;}
void __disable_irq(void){mock_primask=1u;}
void __enable_irq(void){mock_primask=0u;}
void LPIT0_Ch0_IRQHandler(void);
void LPIT0_Ch1_IRQHandler(void);
void LPIT0_Ch2_IRQHandler(void);
void LPIT0_Ch3_IRQHandler(void);
static void counter(void *ctx){++*(uint32_t *)ctx;}
static void irq(uint32_t ch){
    mock_lpit.MSR=1u<<ch;
    switch(ch){case 0:LPIT0_Ch0_IRQHandler();break;case 1:LPIT0_Ch1_IRQHandler();break;
               case 2:LPIT0_Ch2_IRQHandler();break;default:LPIT0_Ch3_IRQHandler();break;}
}
static void reset_mock(void){
    memset(&mock_lpit,0,sizeof(mock_lpit));
    memset(&mock_pcc,0,sizeof(mock_pcc));
    memset(mock_irq_enabled,0,sizeof(mock_irq_enabled));
    memset(callback_counts,0,sizeof(callback_counts));
    memset(pending_clear,0,sizeof(pending_clear));
}
int main(void){
    const TimerManagerConfig cfg={6u,40000000u,TIMER_CH0};
    const TimerManagerConfig no_tick={6u,40000000u,TIMER_CHANNEL_NONE};
    const TimerManagerConfig bad_clock={6u,1000001u,TIMER_CH0};
    assert(TimerManager_Init(NULL)==TIMER_ERR_ARGUMENT);
    assert(TimerManager_Init(&bad_clock)==TIMER_ERR_ARGUMENT);
    assert(TimerManager_Init(&cfg)==TIMER_OK);
    assert(mock_lpit.TMR[0].TVAL==39999u);
    assert((mock_lpit.TMR[0].TCTRL&1u)==1u);
    assert((mock_pcc.PCCn[PCC_LPIT_INDEX]&PCC_PCCn_PCS_MASK)==PCC_PCCn_PCS(6));
    assert(TimerManager_Configure(TIMER_CH0,1000u,false)==TIMER_ERR_RESERVED);
    assert(TimerManager_Start(TIMER_CH1)==TIMER_ERR_NOT_CONFIGURED);
    assert(TimerManager_Configure(TIMER_CH2,0u,false)==TIMER_ERR_RANGE);
    assert(TimerManager_Configure(TIMER_CH2,UINT32_MAX,false)==TIMER_ERR_RANGE);
    assert(TimerManager_Configure(TIMER_CHANNEL_NONE,100u,false)==TIMER_ERR_ARGUMENT);
    assert(TimerManager_Configure(TIMER_CH1,2500u,false)==TIMER_OK);
    assert(TimerManager_RegisterCallback(TIMER_CH1,counter,&callback_counts[1])==TIMER_OK);
    assert(TimerManager_Start(TIMER_CH1)==TIMER_OK);
    assert(TimerManager_RegisterCallback(TIMER_CH1,counter,&callback_counts[1])==TIMER_ERR_BUSY);
    irq(1);irq(1);assert(callback_counts[1]==2u);
    assert(TimerManager_Stop(TIMER_CH1)==TIMER_OK);
    assert(TimerManager_Configure(TIMER_CH3,10u,true)==TIMER_OK);
    assert(mock_lpit.TMR[3].TVAL==399u);
    assert((mock_lpit.TMR[3].TCTRL&(1u<<17))!=0u);
    assert((mock_lpit.TMR[3].TCTRL&(1u<<16))==0u);
    assert(TimerManager_RegisterCallback(TIMER_CH3,counter,&callback_counts[3])==TIMER_OK);
    assert(TimerManager_Start(TIMER_CH3)==TIMER_OK);
    irq(3);assert(callback_counts[3]==1u);
    mock_lpit.TMR[3].TCTRL &= ~1u; /* mock simulated timeout stop */
    assert(TimerManager_Start(TIMER_CH3)==TIMER_OK);
    irq(3);assert(callback_counts[3]==2u);
    for(uint32_t i=1;i<=200;i++){irq(0);assert(TimerManager_GetTickMs()==i);}
    assert(TimerManager_Init(&no_tick)==TIMER_OK);
    assert(TimerManager_GetTickMs()==0u);
    assert(TimerManager_Configure(TIMER_CH0,1000u,false)==TIMER_OK);
    assert(TimerManager_Configure(TIMER_CH1,1000u,false)==TIMER_OK);
    assert(TimerManager_Configure(TIMER_CH2,1000u,false)==TIMER_OK);
    assert(TimerManager_Configure(TIMER_CH3,1000u,false)==TIMER_OK);
    reset_mock();
    puts("PASS: selectable tick CH0, general-purpose CH1/2/3, one-shot, no-tick mode");
    return 0;
}
