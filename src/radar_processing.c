#include "radar_processing.h"
#include <stddef.h>
bool RadarProcessing_Init(RadarProcessing *p,int32_t min,int32_t max,int32_t step,
                          uint8_t window,uint32_t threshold,
                          const TargetDetectorConfig *dc,int32_t ma,uint32_t mr,
                          uint32_t lost_ms,uint8_t misses) {
    if(!p||!dc||!RadarFrame_Init(&p->frame,min,max,step))return false;
    if(!BackgroundFilter_Init(&p->background,p->frame.bins,threshold))return false;
    if(!TargetDetector_Init(&p->detector,dc)||
       !TargetTracker_Init(&p->tracker,ma,mr,lost_ms,misses))return false;
    for(uint16_t i=0;i<p->frame.bins;i++)
        if(!DistanceFilter_Init(&p->filters[i],window))return false;
    p->display_frame=p->frame;
    p->candidate_count=0u;p->background_ready=0u;p->calibrating=0u;return true;
}
void RadarProcessing_RequestCalibration(RadarProcessing *p) {
    if(!p)return;
    BackgroundFilter_Reset(&p->background);
    p->background_ready=0u;p->calibrating=1u;
}
bool RadarProcessing_PushSample(RadarProcessing *p,const RadarSample *s) {
    if(!p||!s)return false;
    return RadarFrame_Add(&p->frame,s);
}
void RadarProcessing_ProcessCompleted(RadarProcessing *p,uint32_t now) {
    if(!p||!RadarFrame_TakeCompleted(&p->frame))return;
    p->display_frame=p->frame;
    TargetDetector_Clear(&p->detector);
    for(uint16_t i=0;i<p->frame.bins;i++) {
        RadarSample s;uint32_t filtered;bool foreground=false;
        if(!RadarFrame_Get(&p->frame,i,&s)||!s.valid)continue;
        if(!DistanceFilter_Update(&p->filters[i],s.distance_mm,true,&filtered))continue;
        if(p->calibrating) {
            (void)BackgroundFilter_Calibrate(&p->background,i,filtered);
        } else if(p->background_ready &&
                  BackgroundFilter_IsForeground(&p->background,i,filtered,&foreground) &&
                  foreground) {
            TargetObservation o={s.angle_mdeg,filtered,s.timestamp_ms};
            (void)TargetDetector_Add(&p->detector,o);
        }
    }
    if(p->calibrating){p->calibrating=0u;p->background_ready=1u;}
    p->candidate_count=TargetDetector_Detect(&p->detector,p->candidates,DETECTOR_MAX_CANDIDATES);
    TargetTracker_Update(&p->tracker,p->candidates,p->candidate_count,now);
    RadarFrame_Clear(&p->frame);
}
