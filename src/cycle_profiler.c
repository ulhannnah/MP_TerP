#include "cycle_profiler.h"
#include <stddef.h>
#if defined(__arm__) || defined(__thumb__)
#include "device_registers.h"
#define CYCLE_HW 1
#else
#define CYCLE_HW 0
#endif
bool CycleProfiler_Enable(void){
#if CYCLE_HW
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT=0u;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    return (DWT->CTRL & DWT_CTRL_CYCCNTENA_Msk)!=0u;
#else
    return false;
#endif
}
static uint32_t now_cycles(void){
#if CYCLE_HW
    return DWT->CYCCNT;
#else
    return 0u;
#endif
}
void CycleProfiler_Reset(CycleProfiler *p){
    if(!p)return;
    p->start=0u;p->last=0u;p->maximum=0u;p->total=0u;p->count=0u;
}
void CycleProfiler_Begin(CycleProfiler *p){if(p)p->start=now_cycles();}
void CycleProfiler_End(CycleProfiler *p){
    if(!p)return;
    uint32_t elapsed=now_cycles()-p->start;
    p->last=elapsed;p->total+=elapsed;
    if(elapsed>p->maximum)p->maximum=elapsed;
    if(p->count<UINT32_MAX)++p->count;
}
uint32_t CycleProfiler_Average(const CycleProfiler *p){
    return (!p||p->count==0u)?0u:(uint32_t)(p->total/p->count);
}
