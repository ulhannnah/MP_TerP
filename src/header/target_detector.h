#ifndef TARGET_DETECTOR_H
#define TARGET_DETECTOR_H
#include <stdbool.h>
#include <stdint.h>
#define DETECTOR_MAX_OBSERVATIONS 128u
#define DETECTOR_MAX_CANDIDATES 16u
typedef struct { int32_t angle_mdeg; uint32_t range_mm; uint32_t timestamp_ms; } TargetObservation;
typedef struct {
    int32_t angle_mdeg; uint32_t range_mm; uint32_t timestamp_ms;
    uint16_t samples;
} TargetCandidate;
typedef struct {
    uint32_t max_range_difference_mm;
    int32_t max_angle_gap_mdeg;
    uint16_t min_cluster_samples;
} TargetDetectorConfig;
typedef struct {
    TargetDetectorConfig config;
    TargetObservation observations[DETECTOR_MAX_OBSERVATIONS];
    uint16_t count;
} TargetDetector;
bool TargetDetector_Init(TargetDetector *d,const TargetDetectorConfig *config);
void TargetDetector_Clear(TargetDetector *d);
bool TargetDetector_Add(TargetDetector *d,TargetObservation observation);
uint16_t TargetDetector_Detect(const TargetDetector *d,TargetCandidate *out,uint16_t capacity);
#endif
