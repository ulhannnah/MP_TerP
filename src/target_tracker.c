#include "target_tracker.h"
#include <stddef.h>
bool TargetTracker_Init(TargetTracker *t,int32_t max_angle,uint32_t max_range,uint32_t timeout,uint8_t misses){
    if(!t||max_angle<=0||max_range==0u||timeout==0u||misses==0u)return false;
    t->max_angle_delta_mdeg=max_angle;t->max_range_delta_mm=max_range;
    t->lost_timeout_ms=timeout;t->max_misses=misses;
    for(uint8_t i=0;i<TRACKER_MAX_TARGETS;i++) {
        t->targets[i].id=(uint8_t)(i+1u);
        t->targets[i].state=TARGET_EMPTY;t->targets[i].miss_count=0;
        t->targets[i].last_seen_ms=0;t->targets[i].angle_mdeg=0;t->targets[i].range_mm=0;
    }
    return true;
}
static uint32_t abs_angle(int32_t a,int32_t b){
    return a>=b?(uint32_t)((int64_t)a-b):(uint32_t)((int64_t)b-a);
}
static uint32_t abs_range(uint32_t a,uint32_t b){return a>b?a-b:b-a;}
void TargetTracker_Update(TargetTracker *t,const TargetCandidate *c,uint16_t n,uint32_t now){
    uint8_t matched[TRACKER_MAX_TARGETS]={0};
    if(!t||(n!=0u&&!c))return;
    /* Explicit slot expiry: expired LOST tracks are reusable for new IDs. */
    for(uint8_t j=0;j<TRACKER_MAX_TARGETS;j++) {
        RadarTarget *p=&t->targets[j];
        if(p->state==TARGET_LOST &&
           (uint32_t)(now-p->last_seen_ms)>t->lost_timeout_ms)
            p->state=TARGET_EMPTY;
    }
    for(uint16_t i=0;i<n;i++){
        uint8_t best=TRACKER_MAX_TARGETS;
        uint64_t best_score=UINT64_MAX;
        for(uint8_t j=0;j<TRACKER_MAX_TARGETS;j++){
            RadarTarget *p=&t->targets[j];
            uint32_t da,dr;
            if(matched[j]||p->state==TARGET_EMPTY)continue;
            if((uint32_t)(now-p->last_seen_ms)>t->lost_timeout_ms)continue;
            da=abs_angle(p->angle_mdeg,c[i].angle_mdeg);
            dr=abs_range(p->range_mm,c[i].range_mm);
            if(da>(uint32_t)t->max_angle_delta_mdeg||dr>t->max_range_delta_mm)continue;
            uint64_t score=(uint64_t)da*1000u/(uint32_t)t->max_angle_delta_mdeg+
                           (uint64_t)dr*1000u/t->max_range_delta_mm;
            if(score<best_score){best_score=score;best=j;}
        }
        if(best==TRACKER_MAX_TARGETS) {
            for(uint8_t j=0;j<TRACKER_MAX_TARGETS;j++)
                if(t->targets[j].state==TARGET_EMPTY){best=j;break;}
        }
        if(best==TRACKER_MAX_TARGETS)continue;
        RadarTarget *p=&t->targets[best];
        bool fresh=p->state==TARGET_EMPTY;
        p->angle_mdeg=c[i].angle_mdeg;p->range_mm=c[i].range_mm;
        p->last_seen_ms=now;p->miss_count=0;p->state=fresh?TARGET_NEW:TARGET_TRACKING;
        matched[best]=1u;
    }
    for(uint8_t j=0;j<TRACKER_MAX_TARGETS;j++) {
        RadarTarget *p=&t->targets[j];
        if(p->state==TARGET_EMPTY||matched[j])continue;
        if(p->miss_count<UINT8_MAX)++p->miss_count;
        if(p->miss_count>=t->max_misses ||
           (uint32_t)(now-p->last_seen_ms)>t->lost_timeout_ms)p->state=TARGET_LOST;
    }
}
bool TargetTracker_Get(const TargetTracker *t,uint8_t id,RadarTarget *out){
    if(!t||!out||id==0u||id>TRACKER_MAX_TARGETS)return false;
    *out=t->targets[id-1u];
    return out->state!=TARGET_EMPTY;
}
