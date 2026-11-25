#ifndef MP_TERP_AIM_GEOMETRY_H
#define MP_TERP_AIM_GEOMETRY_H
#include <stdbool.h>
#include <stdint.h>

/* The scan angle is absolute in the scanner's frame. The Stepper uses
 * relative degrees around a MANUALLY confirmed center. These functions
 * transform coordinates only; they never claim the hardware was homed. */
static inline bool MP_TERP_AimToStepper(int32_t scan_angle_mdeg,
                                        int32_t center_angle_mdeg,
                                        int32_t *relative_mdeg)
{
    if(relative_mdeg==0)return false;
    const int64_t result=(int64_t)scan_angle_mdeg-center_angle_mdeg;
    if(result<INT32_MIN || result>INT32_MAX)return false;
    *relative_mdeg=(int32_t)result;
    return true;
}

static inline bool MP_TERP_StepperToAim(int32_t relative_mdeg,
                                        int32_t center_angle_mdeg,
                                        int32_t *scan_angle_mdeg)
{
    if(scan_angle_mdeg==0)return false;
    const int64_t result=(int64_t)relative_mdeg+center_angle_mdeg;
    if(result<INT32_MIN || result>INT32_MAX)return false;
    *scan_angle_mdeg=(int32_t)result;
    return true;
}
#endif
