#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "app_profile.h"
#include "aim_geometry.h"

int main(void)
{
    const AppConfig cfg=MP_TERP_Radar120Profile();
    int32_t converted=0;

    assert(cfg.minimum_scan_angle_mdeg==30000);
    assert(cfg.maximum_scan_angle_mdeg==150000);
    assert(cfg.scan_step_mdeg==5000);
    assert((cfg.maximum_scan_angle_mdeg-cfg.minimum_scan_angle_mdeg)
            /cfg.scan_step_mdeg+1==25);
    assert(cfg.average_window>0u && cfg.detector.min_cluster_samples>=2u);
    assert(cfg.operating_target_limit==3u);
    assert(cfg.aim_max_age_ms < cfg.lost_timeout_ms);
    assert(cfg.lost_timeout_ms >
           2u*25u*(cfg.settle_ms+cfg.echo_timeout_ms));
    assert(cfg.oled_period_ms>0u && cfg.lcd_period_ms>0u);
    assert(cfg.simulation_ms>0u);

    assert(MP_TERP_AimToStepper(30000,90000,&converted) && converted==-60000);
    assert(MP_TERP_AimToStepper(90000,90000,&converted) && converted==0);
    assert(MP_TERP_AimToStepper(150000,90000,&converted) && converted==60000);
    assert(MP_TERP_StepperToAim(-60000,90000,&converted) && converted==30000);
    assert(MP_TERP_StepperToAim(60000,90000,&converted) && converted==150000);
    assert(!MP_TERP_AimToStepper(INT32_MIN,INT32_MAX,&converted));
    assert(!MP_TERP_StepperToAim(INT32_MAX,INT32_MAX,&converted));
    assert(!MP_TERP_AimToStepper(30000,90000,0));
    assert(!MP_TERP_StepperToAim(0,90000,0));

    puts("PASS: Stage5 profile, radar/stepper coordinate map, overflow guards");
    return 0;
}
