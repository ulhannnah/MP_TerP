#ifndef MP_TERP_APP_PROFILE_H
#define MP_TERP_APP_PROFILE_H

#include "app.h"

/* Preliminary 120-degree radar profile for the COUPLED sensor rig.
 * No actual timing/accuracy figures have been measured on the board.
 * Half-sweep ~25 samples; the per-angle settle/echo window can make a
 * revisit take several seconds, so LOST/age values are larger than 1s.
 * This function sets application parameters only, never hardware I/O.
 */
static inline AppConfig MP_TERP_Radar120Profile(void)
{
    AppConfig cfg = {0};
    cfg.minimum_scan_angle_mdeg = 30000;
    cfg.maximum_scan_angle_mdeg = 150000;
    cfg.scan_step_mdeg = 5000;
    cfg.settle_ms = 80u;
    cfg.echo_timeout_ms = 35u;

    cfg.average_window = 3u;
    cfg.background_threshold_mm = 150u;
    cfg.detector.max_range_difference_mm = 250u;
    cfg.detector.max_angle_gap_mdeg = 11000;
    cfg.detector.min_cluster_samples = 2u;
    cfg.match_angle_mdeg = 20000;
    cfg.match_range_mm = 500u;
    cfg.lost_timeout_ms = 10000u;
    cfg.max_misses = 3u;

    cfg.adc_max = 4095u;
    cfg.adc_hysteresis = 20u;
    cfg.operating_target_limit = 3u; /* storage capacity is five */
    cfg.maximum_display_distance_mm = 4000u;
    cfg.oled_period_ms = 150u;
    cfg.lcd_period_ms = 300u;
    cfg.aim_max_age_ms = 8000u;
    cfg.aim_tolerance_mdeg = 2000;
    cfg.simulation_ms = 200u; /* LED only, no physical launcher */
    cfg.button_debounce_ms = 60u;
    return cfg;
}

#endif /* MP_TERP_APP_PROFILE_H */
