#ifndef COORDINATE_MATH_H
#define COORDINATE_MATH_H
#include <stdbool.h>
#include <stdint.h>
typedef struct {float x_mm,y_mm;} Coordinate2D;
typedef struct {float vx_mm_s,vy_mm_s;} Velocity2D;
bool Coordinate_PolarToCartesian(int32_t angle_mdeg,float distance_mm,Coordinate2D *out);
bool Coordinate_CalculateVelocity(const Coordinate2D *previous,const Coordinate2D *current,
                                  uint32_t delta_ms,Velocity2D *out);
bool Coordinate_Predict(const Coordinate2D *position,const Velocity2D *velocity,
                        uint32_t horizon_ms,Coordinate2D *out);
float Coordinate_DistanceSquared(const Coordinate2D *a,const Coordinate2D *b);
#endif
