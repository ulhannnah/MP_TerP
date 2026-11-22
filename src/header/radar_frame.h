#ifndef RADAR_FRAME_H
#define RADAR_FRAME_H
#include <stdbool.h>
#include <stdint.h>
#include "radar_scan.h"
#define RADAR_FRAME_MAX_BINS 128u
typedef struct {
    RadarSample samples[RADAR_FRAME_MAX_BINS];
    uint8_t present[RADAR_FRAME_MAX_BINS];
    uint16_t bins;
    uint16_t received;
    uint32_t frame_number;
    uint8_t complete;
    int32_t first_angle_mdeg;
    int32_t last_angle_mdeg;
    int32_t step_mdeg;
    int8_t direction;
} RadarFrame;
bool RadarFrame_Init(RadarFrame *f,int32_t min_angle_mdeg,int32_t max_angle_mdeg,int32_t step_mdeg);
void RadarFrame_Clear(RadarFrame *f);
bool RadarFrame_Add(RadarFrame *f,const RadarSample *sample);
/* Returns true once on the end of each MIN->MAX or MAX->MIN pass. */
bool RadarFrame_TakeCompleted(RadarFrame *f);
bool RadarFrame_Get(const RadarFrame *f,uint16_t bin,RadarSample *sample);
#endif
