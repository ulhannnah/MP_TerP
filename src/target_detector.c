#include "target_detector.h"
#include <stddef.h>
bool TargetDetector_Init(TargetDetector *d,const TargetDetectorConfig *cfg) {
    if(!d||!cfg||cfg->max_angle_gap_mdeg<=0||cfg->max_range_difference_mm==0u||
       cfg->min_cluster_samples==0u)return false;
    d->config=*cfg;d->count=0;return true;
}
void TargetDetector_Clear(TargetDetector *d){if(d)d->count=0;}
bool TargetDetector_Add(TargetDetector *d,TargetObservation o) {
    if(!d||d->count>=DETECTOR_MAX_OBSERVATIONS||o.range_mm==0u)return false;
    /* Observations are sorted on angle. A reverse scan is supported too. */
    uint16_t i=d->count;
    while(i>0 && d->observations[i-1u].angle_mdeg>o.angle_mdeg) {
        d->observations[i]=d->observations[i-1u];--i;
    }
    d->observations[i]=o;++d->count;return true;
}
uint16_t TargetDetector_Detect(const TargetDetector *d,TargetCandidate *out,uint16_t capacity) {
    uint16_t produced=0;
    if(!d||!out||capacity==0u)return 0;
    for(uint16_t i=0;i<d->count;) {
        uint16_t start=i, n=1u;
        int64_t angle=d->observations[i].angle_mdeg;
        uint64_t range=d->observations[i].range_mm;
        uint32_t time=d->observations[i].timestamp_ms;
        while(i+1u<d->count) {
            const TargetObservation *a=&d->observations[i],*b=&d->observations[i+1u];
            uint32_t dr=a->range_mm>b->range_mm?a->range_mm-b->range_mm:b->range_mm-a->range_mm;
            if(b->angle_mdeg-a->angle_mdeg>d->config.max_angle_gap_mdeg ||
               dr>d->config.max_range_difference_mm)break;
            ++i;++n;angle+=b->angle_mdeg;range+=b->range_mm;
            if((int32_t)(b->timestamp_ms-time)>0)time=b->timestamp_ms;
        }
        if(n>=d->config.min_cluster_samples && produced<capacity) {
            TargetCandidate *c=&out[produced++];
            c->angle_mdeg=(int32_t)(angle/n);
            c->range_mm=(uint32_t)(range/n);
            c->timestamp_ms=time;c->samples=n;
        }
        ++i;
        (void)start;
    }
    return produced;
}
