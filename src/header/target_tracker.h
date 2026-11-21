#ifndef TARGET_TRACKER_H
#define TARGET_TRACKER_H
#include <stdbool.h>
#include <stdint.h>
#include "target_detector.h"
#define TRACKER_MAX_TARGETS 5u
typedef enum { TARGET_EMPTY, TARGET_NEW, TARGET_TRACKING, TARGET_LOST } TargetState;
typedef struct {
    uint8_t id;
    TargetState state;
    int32_t angle_mdeg;
    uint32_t range_mm;
    uint32_t last_seen_ms;
    uint8_t miss_count;
} RadarTarget;
typedef struct {
    RadarTarget targets[TRACKER_MAX_TARGETS];
    int32_t max_angle_delta_mdeg;
    uint32_t max_range_delta_mm;
    uint32_t lost_timeout_ms;
    uint8_t max_misses;
} TargetTracker;
bool TargetTracker_Init(TargetTracker *t,int32_t max_angle_delta_mdeg,uint32_t max_range_delta_mm,uint32_t lost_timeout_ms,uint8_t max_misses);
/* Call once per completed sweep; greedily associates candidates; unmatched become LOST. */
void TargetTracker_Update(TargetTracker *t,const TargetCandidate *c,uint16_t n,uint32_t now_ms);
bool TargetTracker_Get(const TargetTracker *t,uint8_t id,RadarTarget *out);
#endif
