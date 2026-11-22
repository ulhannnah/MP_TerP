#include "radar_frame.h"
#include <stddef.h>
bool RadarFrame_Init(RadarFrame *f,int32_t min,int32_t max,int32_t step) {
    int64_t span,bins;
    if(!f||step<=0||min>=max)return false;
    span=(int64_t)max-min;
    bins=span/step+1+(span%step!=0);
    if(bins>RADAR_FRAME_MAX_BINS)return false;
    f->bins=(uint16_t)bins;
    f->first_angle_mdeg=min;
    f->step_mdeg=step;
    f->frame_number=0u;
    f->direction=1;
    RadarFrame_Clear(f);
    return true;
}
void RadarFrame_Clear(RadarFrame *f) {
    if(!f)return;
    f->received=0u;f->complete=0u;
    for(unsigned i=0;i<RADAR_FRAME_MAX_BINS;i++)f->present[i]=0u;
}
bool RadarFrame_Add(RadarFrame *f,const RadarSample *sample) {
    int64_t offset,idx;
    if(!f||!sample||!f->bins)return false;
    offset=(int64_t)sample->angle_mdeg-f->first_angle_mdeg;
    if(offset<0)return false;
    idx=(offset+f->step_mdeg/2)/f->step_mdeg;
    if(idx<0||idx>=f->bins)return false;
    if(!f->present[idx])++f->received;
    f->samples[idx]=*sample;f->present[idx]=1u;
    /* A complete half-sweep has both endpoints; avoid initial MIN alone. */
    if(f->received>1u &&
       ((f->direction>0 && idx==f->bins-1) ||
        (f->direction<0 && idx==0))) {
        f->complete=1u;++f->frame_number;
        f->direction=(int8_t)-f->direction;
        return true;
    }
    return false;
}
bool RadarFrame_TakeCompleted(RadarFrame *f){
    if(!f||!f->complete)return false;
    f->complete=0u;return true;
}
bool RadarFrame_Get(const RadarFrame *f,uint16_t bin,RadarSample *sample){
    if(!f||!sample||bin>=f->bins||!f->present[bin])return false;
    *sample=f->samples[bin];return true;
}
