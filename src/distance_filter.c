#include "distance_filter.h"
#include <stddef.h>
bool DistanceFilter_Init(DistanceFilter *f,uint8_t window){
    if(!f||window==0u||window>DISTANCE_FILTER_CAPACITY)return false;
    f->window=window;DistanceFilter_Reset(f);return true;
}
void DistanceFilter_Reset(DistanceFilter *f){
    if(!f)return;
    f->sum=0;f->count=0;f->next=0;
    for(unsigned i=0;i<DISTANCE_FILTER_CAPACITY;i++)f->samples[i]=0;
}
bool DistanceFilter_Update(DistanceFilter *f,uint32_t mm,bool valid,uint32_t *out){
    if(!f||!out||f->window==0u||!valid)return false;
    if(f->count==f->window)f->sum-=f->samples[f->next];
    else ++f->count;
    f->samples[f->next]=mm;f->sum+=mm;
    f->next=(uint8_t)((f->next+1u)%f->window);
    *out=(uint32_t)((f->sum+f->count/2u)/f->count);return true;
}
