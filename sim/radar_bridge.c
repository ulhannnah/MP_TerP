/*
 * Host-only bridge to exercise the production C radar scan and processing
 * modules from Python ctypes. Do NOT add sim/ to the S32DS build.
 */
#include <stdbool.h>
#include <stdint.h>
#include "radar_scan.h"
#include "radar_processing.h"

static RadarScan scan;
static RadarProcessing processing;
static uint8_t echo_requested, echo_ready, echo_valid;
static uint32_t echo_distance;
static int32_t motor_angle;
static uint32_t frames_processed, samples_seen;

static bool motor_move(void *context, int32_t angle)
{
    (void)context;
    motor_angle = angle;
    return true;
}
static bool echo_trigger(void *context, uint32_t now_ms)
{
    (void)context;
    (void)now_ms;
    echo_requested = 1u;
    echo_ready = 0u;
    return true;
}
static bool echo_take(void *context, uint32_t *mm, bool *valid)
{
    (void)context;
    if (!echo_ready) return false;
    *mm = echo_distance;
    *valid = (echo_valid != 0u);
    echo_ready = 0u;
    return true;
}

int sim_init(int32_t min_angle, int32_t max_angle, int32_t step_angle,
             uint32_t settle_ms, uint32_t timeout_ms, uint8_t avg_window,
             uint32_t background_threshold, uint16_t min_cluster_samples,
             int32_t max_cluster_gap, uint32_t max_cluster_delta,
             int32_t max_match_angle, uint32_t max_match_range,
             uint32_t lost_timeout_ms, uint8_t max_misses)
{
    RadarScanConfig sc;
    TargetDetectorConfig dc;
    dc.max_angle_gap_mdeg = max_cluster_gap;
    dc.max_range_difference_mm = max_cluster_delta;
    dc.min_cluster_samples = min_cluster_samples;
    sc.min_angle_mdeg = min_angle;
    sc.max_angle_mdeg = max_angle;
    sc.step_mdeg = step_angle;
    sc.settle_ms = settle_ms;
    sc.echo_timeout_ms = timeout_ms;
    sc.move = motor_move;
    sc.trigger = echo_trigger;
    sc.take = echo_take;
    sc.context = 0;
    if (!RadarProcessing_Init(&processing, min_angle, max_angle, step_angle,
                              avg_window, background_threshold, &dc,
                              max_match_angle, max_match_range,
                              lost_timeout_ms, max_misses)) return 0;
    if (!RadarScan_Init(&scan, &sc)) return 0;
    RadarProcessing_RequestCalibration(&processing);
    echo_requested = echo_ready = echo_valid = 0u;
    echo_distance = frames_processed = samples_seen = 0u;
    motor_angle = min_angle;
    return 1;
}
int sim_start(uint32_t now_ms)
{
    return RadarScan_Start(&scan, now_ms) ? 1 : 0;
}
int sim_step(uint32_t now_ms)
{
    RadarSample sample;
    uint32_t previous = frames_processed;
    RadarScan_Update(&scan, now_ms);
    if (RadarScan_TakeSample(&scan, &sample)) {
        samples_seen++;
        if (RadarProcessing_PushSample(&processing, &sample)) {
            RadarProcessing_ProcessCompleted(&processing, now_ms);
            frames_processed++;
        }
    }
    return frames_processed != previous;
}
int sim_echo_requested(void) { return echo_requested != 0u; }
void sim_inject_echo(uint32_t mm, int valid)
{
    if (!echo_requested) return;
    echo_requested = 0u;
    echo_distance = mm;
    echo_valid = valid ? 1u : 0u;
    echo_ready = 1u;
}
int32_t sim_scan_angle(void) { return motor_angle; }
uint32_t sim_frame_count(void) { return frames_processed; }
uint32_t sim_sample_count(void) { return samples_seen; }
int sim_background_ready(void) { return processing.background_ready != 0u; }
uint16_t sim_candidate_count(void) { return processing.candidate_count; }
int sim_candidate(uint16_t index, int32_t *angle, uint32_t *distance, uint16_t *count)
{
    if (index >= processing.candidate_count || !angle || !distance || !count)
        return 0;
    *angle = processing.candidates[index].angle_mdeg;
    *distance = processing.candidates[index].range_mm;
    *count = processing.candidates[index].samples;
    return 1;
}
int sim_target(uint8_t id, int *state, int32_t *angle, uint32_t *distance,
               uint32_t *last_seen_ms)
{
    RadarTarget t;
    if (!state || !angle || !distance || !last_seen_ms ||
        !TargetTracker_Get(&processing.tracker, id, &t)) return 0;
    *state = (int)t.state;
    *angle = t.angle_mdeg;
    *distance = t.range_mm;
    *last_seen_ms = t.last_seen_ms;
    return 1;
}
int sim_frame_bin(uint16_t index, int32_t *angle, uint32_t *distance, int *valid)
{
    RadarSample sample;
    if (!angle || !distance || !valid ||
        !RadarFrame_Get(&processing.display_frame, index, &sample)) return 0;
    *angle = sample.angle_mdeg;
    *distance = sample.distance_mm;
    *valid = sample.valid ? 1 : 0;
    return 1;
}
uint16_t sim_frame_bins(void) { return processing.display_frame.bins; }
