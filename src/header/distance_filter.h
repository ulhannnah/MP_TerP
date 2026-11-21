#ifndef DISTANCE_FILTER_H
#define DISTANCE_FILTER_H
#include <stdbool.h>
#include <stdint.h>
#define DISTANCE_FILTER_CAPACITY 9u
typedef struct {
    uint32_t samples[DISTANCE_FILTER_CAPACITY];
    uint64_t sum;
    uint8_t window,count,next;
} DistanceFilter;
bool DistanceFilter_Init(DistanceFilter *filter,uint8_t window);
void DistanceFilter_Reset(DistanceFilter *filter);
bool DistanceFilter_Update(DistanceFilter *filter,uint32_t input_mm,bool valid,uint32_t *output_mm);
#endif
