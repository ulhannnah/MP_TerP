#ifndef TASK_SCHEDULER_H
#define TASK_SCHEDULER_H
#include <stdbool.h>
#include <stdint.h>
#define SCHEDULER_MAX_TASKS 12u
typedef void (*SchedulerTask)(void *context,uint32_t now_ms);
typedef struct {
    SchedulerTask callback;
    void *context;
    uint32_t period_ms;
    uint32_t next_ms;
    uint32_t runs;
    uint8_t active;
} SchedulerEntry;
typedef struct {
    SchedulerEntry task[SCHEDULER_MAX_TASKS];
    uint8_t count;
} TaskScheduler;
/* Does not initialize or own the system timer. */
void TaskScheduler_Init(TaskScheduler *scheduler);
bool TaskScheduler_Add(TaskScheduler *scheduler,SchedulerTask callback,void *context,uint32_t period_ms,uint32_t first_ms);
void TaskScheduler_Run(TaskScheduler *scheduler,uint32_t now_ms);
#endif
