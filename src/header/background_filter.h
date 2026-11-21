#ifndef BACKGROUND_FILTER_H
#define BACKGROUND_FILTER_H
#include <stdbool.h>
#include <stdint.h>
#define BACKGROUND_MAX_BINS 128u
typedef struct {
    uint32_t baseline_mm[BACKGROUND_MAX_BINS];
    uint8_t known[BACKGROUND_MAX_BINS];
    uint16_t bins;
    uint32_t difference_threshold_mm;
} BackgroundFilter;
bool BackgroundFilter_Init(BackgroundFilter *f,uint16_t bins,uint32_t threshold_mm);
bool BackgroundFilter_Calibrate(BackgroundFilter *f,uint16_t bin,uint32_t distance_mm);
bool BackgroundFilter_IsForeground(const BackgroundFilter *f,uint16_t bin,uint32_t mm,bool *foreground);
void BackgroundFilter_Reset(BackgroundFilter *f);
#endif
