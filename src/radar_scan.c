#include "radar_scan.h"
#include <stddef.h>
bool RadarScan_Init(RadarScan *s,const RadarScanConfig *cfg) {
    if(!s||!cfg||!cfg->move||!cfg->trigger||!cfg->take||
       cfg->step_mdeg<=0||cfg->min_angle_mdeg>=cfg->max_angle_mdeg||
       cfg->settle_ms==0||cfg->echo_timeout_ms==0||
       cfg->step_mdeg>cfg->max_angle_mdeg-cfg->min_angle_mdeg)return false;
    s->cfg=*cfg; s->state=RADAR_IDLE; s->direction=1;
    s->angle_mdeg=cfg->min_angle_mdeg; s->pending=0; s->samples=0; return true;
}
bool RadarScan_Start(RadarScan *s,uint32_t now) {
    if(!s||s->state!=RADAR_IDLE||!s->cfg.move(s->cfg.context,s->angle_mdeg))return false;
    s->started_ms=now; s->state=RADAR_SETTLE; return true;
}
void RadarScan_Update(RadarScan *s,uint32_t now) {
    uint32_t distance=0;
    bool valid=false, ready=false;
    if(!s||s->state==RADAR_IDLE)return;
    if(s->state==RADAR_SETTLE) {
        if((uint32_t)(now-s->started_ms)>=s->cfg.settle_ms &&
           s->cfg.trigger(s->cfg.context,now)) {
            s->state=RADAR_WAIT_ECHO;s->started_ms=now;
        }
        return;
    }
    ready=s->cfg.take(s->cfg.context,&distance,&valid);
    if(!ready && (uint32_t)(now-s->started_ms)<s->cfg.echo_timeout_ms)return;
    if(!ready){valid=false;distance=0;}
    s->last.angle_mdeg=s->angle_mdeg;
    s->last.distance_mm=distance;
    s->last.timestamp_ms=now;
    s->last.valid=valid;
    s->pending=1u; ++s->samples;
    /* Reach both physical endpoints even when step does not divide range. */
    {
        int64_t next=(int64_t)s->angle_mdeg+
                     (int64_t)s->direction*s->cfg.step_mdeg;
        if(next>s->cfg.max_angle_mdeg) {
            next=s->cfg.max_angle_mdeg;s->direction=-1;
        } else if(next<s->cfg.min_angle_mdeg) {
            next=s->cfg.min_angle_mdeg;s->direction=1;
        }
        /* Never command the same endpoint twice. */
        if((int32_t)next==s->angle_mdeg)
            next=(int64_t)s->angle_mdeg+
                 (int64_t)s->direction*s->cfg.step_mdeg;
        s->angle_mdeg=(int32_t)next;
    }
    if(s->cfg.move(s->cfg.context,s->angle_mdeg)) {
        s->started_ms=now; s->state=RADAR_SETTLE;
    } else s->state=RADAR_IDLE;
}
bool RadarScan_TakeSample(RadarScan *s,RadarSample *sample){
    if(!s||!sample||!s->pending)return false;
    *sample=s->last;s->pending=0;return true;
}
void RadarScan_Stop(RadarScan *s){if(s)s->state=RADAR_IDLE;}
