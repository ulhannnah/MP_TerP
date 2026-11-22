#include "coordinate_math.h"
#include <math.h>
#include <stddef.h>
#define PI_OVER_180000 (3.14159265358979323846f/180000.0f)
bool Coordinate_PolarToCartesian(int32_t angle,float distance,Coordinate2D *out){
    if(!out||!isfinite(distance)||distance<0.0f)return false;
    float radians=(float)angle*PI_OVER_180000;
    out->x_mm=distance*cosf(radians);
    out->y_mm=distance*sinf(radians);
    return true;
}
bool Coordinate_CalculateVelocity(const Coordinate2D *previous,const Coordinate2D *current,
                                  uint32_t delta_ms,Velocity2D *out){
    if(!previous||!current||!out||delta_ms==0u)return false;
    float time=(float)delta_ms*0.001f;
    out->vx_mm_s=(current->x_mm-previous->x_mm)/time;
    out->vy_mm_s=(current->y_mm-previous->y_mm)/time;
    return true;
}
bool Coordinate_Predict(const Coordinate2D *position,const Velocity2D *velocity,
                        uint32_t horizon_ms,Coordinate2D *out){
    if(!position||!velocity||!out)return false;
    float time=(float)horizon_ms*0.001f;
    out->x_mm=position->x_mm+velocity->vx_mm_s*time;
    out->y_mm=position->y_mm+velocity->vy_mm_s*time;
    return true;
}
float Coordinate_DistanceSquared(const Coordinate2D *a,const Coordinate2D *b){
    if(!a||!b)return NAN;
    float dx=a->x_mm-b->x_mm,dy=a->y_mm-b->y_mm;
    return dx*dx+dy*dy;
}
