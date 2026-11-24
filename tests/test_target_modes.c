#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "target_selector.h"
#include "target_tracker.h"

static void selector_checks(uint8_t limit)
{
    TargetSelector s;
    assert(TargetSelector_InitWithLimit(&s,4095u,20u,limit));
    assert(TargetSelector_GetId(&s)==1u);
    /* Sweep the ADC range from 0 to full scale, validating allowed IDs. */
    for(uint32_t raw=0;raw<=4095u;raw++) {
        uint8_t id=TargetSelector_Update(&s,(uint16_t)raw);
        assert(id>=1u && id<=limit);
    }
    assert(TargetSelector_GetId(&s)==limit);
    for(int32_t raw=4095;raw>=0;raw--) {
        uint8_t id=TargetSelector_Update(&s,(uint16_t)raw);
        assert(id>=1u && id<=limit);
    }
    assert(TargetSelector_GetId(&s)==1u);
}

static void tracking_limit_checks(uint8_t limit)
{
    TargetTracker t;
    TargetCandidate candidates[TRACKER_MAX_TARGETS];
    RadarTarget target;
    assert(TargetTracker_Init(&t,20000,1000u,1000u,2u));
    assert(t.active_limit==TRACKER_MAX_TARGETS);
    assert(TargetTracker_SetLimit(&t,limit));
    for(uint8_t i=0;i<TRACKER_MAX_TARGETS;i++) {
        candidates[i].angle_mdeg=30000+(int32_t)i*25000;
        candidates[i].range_mm=1000u+(uint32_t)i*200u;
        candidates[i].timestamp_ms=100u;
        candidates[i].samples=2u;
    }
    TargetTracker_Update(&t,candidates,TRACKER_MAX_TARGETS,100u);
    for(uint8_t id=1;id<=limit;id++) {
        assert(TargetTracker_Get(&t,id,&target));
        assert(target.state==TARGET_NEW);
    }
    for(uint8_t id=limit+1;id<=TRACKER_MAX_TARGETS;id++) {
        assert(!TargetTracker_Get(&t,id,&target));
    }
    /* No mode switch while tracks are active. */
    assert(!TargetTracker_SetLimit(&t,TRACKER_MAX_TARGETS));
    TargetTracker_Update(&t,candidates,TRACKER_MAX_TARGETS,200u);
    for(uint8_t id=1;id<=limit;id++) {
        assert(TargetTracker_Get(&t,id,&target));
        assert(target.state==TARGET_TRACKING);
    }
}

int main(void)
{
    TargetSelector s;
    assert(!TargetSelector_InitWithLimit(&s,4095u,20u,0u));
    assert(!TargetSelector_InitWithLimit(&s,4095u,20u,6u));
    selector_checks(3u);
    selector_checks(5u);
    assert(TargetSelector_Init(&s,4095u,20u));
    assert(s.selectable_targets==5u);
    tracking_limit_checks(3u);
    tracking_limit_checks(5u);
    puts("PASS: default 3-target and extended 5-target selector/tracker limits");
    return 0;
}
