#ifndef CYCLE_PROFILER_H
#define CYCLE_PROFILER_H
#include <stdbool.h>
#include <stdint.h>
typedef struct {uint32_t start;uint32_t last;uint32_t maximum;uint64_t total;uint32_t count;} CycleProfiler;
bool CycleProfiler_Enable(void); /* DWT CYCCNT on Cortex-M4F, false on host */
void CycleProfiler_Reset(CycleProfiler *p);
void CycleProfiler_Begin(CycleProfiler *p);
void CycleProfiler_End(CycleProfiler *p);
uint32_t CycleProfiler_Average(const CycleProfiler *p);
#endif
