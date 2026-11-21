#ifndef RADAR_SCAN_H
#define RADAR_SCAN_H
#include <stdbool.h>
#include <stdint.h>
typedef enum { RADAR_IDLE, RADAR_SETTLE, RADAR_WAIT_ECHO } RadarScanState;
typedef struct {
    int32_t angle_mdeg;
    uint32_t distance_mm;
    uint32_t timestamp_ms;
    bool valid;
} RadarSample;
typedef bool (*RadarMoveServo)(void *context,int32_t angle_mdeg);
typedef bool (*RadarBeginEcho)(void *context,uint32_t now_ms);
typedef bool (*RadarReadEcho)(void *context,uint32_t *distance_mm,bool *valid);
typedef struct {
    int32_t min_angle_mdeg, max_angle_mdeg, step_mdeg;
    uint32_t settle_ms, echo_timeout_ms;
    RadarMoveServo move;
    RadarBeginEcho trigger;
    RadarReadEcho take;
    void *context;
} RadarScanConfig;
typedef struct {
    RadarScanConfig cfg;
    RadarScanState state;
    int32_t angle_mdeg;
    int8_t direction;
    uint32_t started_ms;
    RadarSample last;
    uint8_t pending;
    uint32_t samples;
} RadarScan;
bool RadarScan_Init(RadarScan *scan,const RadarScanConfig *cfg);
bool RadarScan_Start(RadarScan *scan,uint32_t now_ms);
void RadarScan_Update(RadarScan *scan,uint32_t now_ms);
bool RadarScan_TakeSample(RadarScan *scan,RadarSample *sample);
void RadarScan_Stop(RadarScan *scan);
#endif
