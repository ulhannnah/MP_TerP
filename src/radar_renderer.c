#include "radar_renderer.h"
#include "oled.h"
#include <math.h>
#include <stddef.h>
#define RADAR_PI 3.14159265358979323846f
static int16_t map_x(const RadarRendererConfig *cfg,int32_t angle,uint32_t distance) {
    float radians=(float)angle*(RADAR_PI/180000.0f);
    float rr=(float)cfg->radius*(float)distance/(float)cfg->maximum_distance_mm;
    return (int16_t)((float)cfg->center_x+rr*cosf(radians)+0.5f);
}
static int16_t map_y(const RadarRendererConfig *cfg,int32_t angle,uint32_t distance) {
    float radians=(float)angle*(RADAR_PI/180000.0f);
    float rr=(float)cfg->radius*(float)distance/(float)cfg->maximum_distance_mm;
    return (int16_t)((float)cfg->center_y-rr*sinf(radians)+0.5f);
}
bool RadarRenderer_Draw(const RadarRendererConfig *cfg,const RadarFrame *frame,
                        const TargetTracker *tracker,uint8_t selected,int32_t angle) {
    if(!cfg||!tracker||!cfg->maximum_distance_mm||cfg->radius==0u||
       cfg->min_angle_mdeg>=cfg->max_angle_mdeg)return false;
    OLED_Clear();
    /* Arc approximated by short line segments to avoid large trigonometric LUT. */
    for(unsigned band=1;band<=3;band++){
        uint32_t dist=(cfg->maximum_distance_mm*band)/3u;
        for(int32_t a=cfg->min_angle_mdeg+5000;a<=cfg->max_angle_mdeg;a+=5000){
            int32_t prev=a-5000;
            OLED_DrawLine(map_x(cfg,prev,dist),map_y(cfg,prev,dist),
                          map_x(cfg,a,dist),map_y(cfg,a,dist),OLED_WHITE);
        }
    }
    OLED_DrawLine(cfg->center_x,cfg->center_y,
                  map_x(cfg,cfg->min_angle_mdeg,cfg->maximum_distance_mm),
                  map_y(cfg,cfg->min_angle_mdeg,cfg->maximum_distance_mm),OLED_WHITE);
    OLED_DrawLine(cfg->center_x,cfg->center_y,
                  map_x(cfg,cfg->max_angle_mdeg,cfg->maximum_distance_mm),
                  map_y(cfg,cfg->max_angle_mdeg,cfg->maximum_distance_mm),OLED_WHITE);
    OLED_DrawLine(cfg->center_x,cfg->center_y,
                  map_x(cfg,angle,cfg->maximum_distance_mm),
                  map_y(cfg,angle,cfg->maximum_distance_mm),OLED_WHITE);
    if(frame) {
        for(uint16_t i=0;i<frame->bins;i++) {
            RadarSample s;
            if(!RadarFrame_Get(frame,i,&s)||!s.valid)continue;
            uint32_t dist=s.distance_mm;
            if(dist>cfg->maximum_distance_mm)continue;
            OLED_DrawPixel(map_x(cfg,s.angle_mdeg,dist),
                           map_y(cfg,s.angle_mdeg,dist),OLED_WHITE);
        }
    }
    for(uint8_t id=1;id<=TRACKER_MAX_TARGETS;id++) {
        RadarTarget t;
        if(!TargetTracker_Get(tracker,id,&t)||t.state==TARGET_LOST||
           t.range_mm>cfg->maximum_distance_mm)continue;
        int16_t x=map_x(cfg,t.angle_mdeg,t.range_mm);
        int16_t y=map_y(cfg,t.angle_mdeg,t.range_mm);
        if(id==selected)OLED_DrawRect(x-3,y-3,7,7,OLED_WHITE);
        else OLED_FillRect(x-1,y-1,3,3,OLED_WHITE);
        if(x>=0&&x<122&&y>=8&&y<56)
            OLED_DrawChar(x+3,y-3,(char)('0'+id),1u,OLED_WHITE);
    }
    return true;
}
