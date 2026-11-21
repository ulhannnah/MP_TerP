#include "task_scheduler.h"
#include <stddef.h>
void TaskScheduler_Init(TaskScheduler *s) {
    if(!s)return;
    s->count=0;
    for(unsigned i=0;i<SCHEDULER_MAX_TASKS;i++) {
        s->task[i].callback=NULL;s->task[i].active=0;s->task[i].runs=0;
        s->task[i].context=NULL;s->task[i].period_ms=0;s->task[i].next_ms=0;
    }
}
bool TaskScheduler_Add(TaskScheduler *s,SchedulerTask cb,void *ctx,uint32_t period,uint32_t first) {
    if(!s||!cb||period==0u||period>=0x80000000u||s->count>=SCHEDULER_MAX_TASKS)return false;
    SchedulerEntry *e=&s->task[s->count++];
    e->callback=cb;e->context=ctx;e->period_ms=period;e->next_ms=first;
    e->runs=0;e->active=1u;return true;
}
void TaskScheduler_Run(TaskScheduler *s,uint32_t now) {
    if(!s)return;
    for(uint8_t i=0;i<s->count;i++) {
        SchedulerEntry *e=&s->task[i];
        if(!e->active || (int32_t)(now-e->next_ms)<0)continue;
        /* Do not replay an unbounded backlog: execute once per main-loop pass. */
        uint32_t late=(uint32_t)(now-e->next_ms);
        e->next_ms+=(late/e->period_ms+1u)*e->period_ms;
        ++e->runs;e->callback(e->context,now);
    }
}
