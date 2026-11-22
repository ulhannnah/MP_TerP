#ifndef RADAR_RENDERER_H
#define RADAR_RENDERER_H
#include <stdbool.h>
#include <stdint.h>
#include "radar_frame.h"
#include "target_tracker.h"
typedef struct {
    int32_t min_angle_mdeg;
    int32_t max_angle_mdeg;
    uint32_t maximum_distance_mm;
    uint8_t center_x;
    uint8_t center_y;
    uint8_t radius;
} RadarRendererConfig;
bool RadarRenderer_Draw(const RadarRendererConfig *cfg,
                        const RadarFrame *frame,
                        const TargetTracker *tracker,
                        uint8_t selected_id,int32_t scan_angle_mdeg);
#endif
