#include "background_filter.h"
#include <stddef.h>
bool BackgroundFilter_Init(BackgroundFilter *f,uint16_t bins,uint32_t threshold){
    if(!f||bins==0u||bins>BACKGROUND_MAX_BINS||threshold==0u)return false;
    f->bins=bins;f->difference_threshold_mm=threshold;BackgroundFilter_Reset(f);return true;
}
void BackgroundFilter_Reset(BackgroundFilter *f){
    if(!f)return;
    for(unsigned i=0;i<BACKGROUND_MAX_BINS;i++){f->baseline_mm[i]=0;f->known[i]=0;}
}
bool BackgroundFilter_Calibrate(BackgroundFilter *f,uint16_t bin,uint32_t mm){
    if(!f||bin>=f->bins||mm==0u)return false;
    f->baseline_mm[bin]=mm;f->known[bin]=1u;return true;
}
bool BackgroundFilter_IsForeground(const BackgroundFilter *f,uint16_t bin,uint32_t mm,bool *fg){
    uint32_t base;
    if(!f||!fg||bin>=f->bins||!f->known[bin]||mm==0u)return false;
    base=f->baseline_mm[bin];
    *fg=(mm>base?mm-base:base-mm)>=f->difference_threshold_mm;
    return true;
}
