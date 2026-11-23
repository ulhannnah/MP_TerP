#ifndef MP_TRIG_H_
#define MP_TRIG_H_
#include <stdbool.h>
#include <stdint.h>

/* Angles in millidegrees; portable, libm-free trigonometry.
 * Max approximation error is well below 1e-4 on the unit circle.
 */
bool MP_TrigSinCosMdeg(int32_t angle_mdeg, float *sine, float *cosine);

#endif
