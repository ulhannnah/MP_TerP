#include "aim_controller.h"
#include <stddef.h>
bool AimController_Init(AimController *a,const AimControllerConfig *cfg) {
    if(!a||!cfg||!cfg->move||cfg->max_target_age_ms==0u||
       cfg->minimum_angle_mdeg>=cfg->maximum_angle_mdeg)return false;
    a->config=*cfg;a->state=AIM_IDLE;a->selected_id=0u;a->last_commanded_angle_mdeg=0;
    return true;
}
void AimController_Select(AimController *a,uint8_t id) {
    if(!a)return;
    a->selected_id=(id>=1u&&id<=TRACKER_MAX_TARGETS)?id:0u;
    a->state=AIM_IDLE;
}
void AimController_Update(AimController *a,const TargetTracker *tracker,uint32_t now){
    RadarTarget target;
    if(!a||!tracker||a->selected_id==0u)return;
    if(!TargetTracker_Get(tracker,a->selected_id,&target)||
       target.state==TARGET_LOST||target.state==TARGET_EMPTY||
       (uint32_t)(now-target.last_seen_ms)>a->config.max_target_age_ms) {
        a->state=AIM_TARGET_LOST;return;
    }
    if(target.angle_mdeg<a->config.minimum_angle_mdeg||
       target.angle_mdeg>a->config.maximum_angle_mdeg) {
        a->state=AIM_OUT_OF_RANGE;return;
    }
    if(a->config.move(a->config.context,target.angle_mdeg)) {
        a->last_commanded_angle_mdeg=target.angle_mdeg;
        a->state=AIM_TRACKING;
    }
}
