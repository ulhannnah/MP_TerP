#ifndef RADAR_PROCESSING_H
#define RADAR_PROCESSING_H
#include <stdbool.h>
#include <stdint.h>
#include "radar_frame.h"
#include "distance_filter.h"
#include "background_filter.h"
#include "target_detector.h"
#include "target_tracker.h"
typedef struct {
    RadarFrame frame;
    RadarFrame display_frame; /* last completed frame, retained for drawing */
    DistanceFilter filters[RADAR_FRAME_MAX_BINS];
    BackgroundFilter background;
    TargetDetector detector;
    TargetTracker tracker;
    TargetCandidate candidates[DETECTOR_MAX_CANDIDATES];
    uint16_t candidate_count;
    uint8_t background_ready;
    uint8_t calibrating;
} RadarProcessing;
bool RadarProcessing_Init(RadarProcessing *p,int32_t min_angle,int32_t max_angle,
                          int32_t step_angle,uint8_t average_window,
                          uint32_t background_threshold_mm,
                          const TargetDetectorConfig *detector,
                          int32_t match_angle,uint32_t match_range,
                          uint32_t lost_timeout_ms,uint8_t max_misses);
void RadarProcessing_RequestCalibration(RadarProcessing *p);
bool RadarProcessing_PushSample(RadarProcessing *p,const RadarSample *sample);
void RadarProcessing_ProcessCompleted(RadarProcessing *p,uint32_t now_ms);
#endif
