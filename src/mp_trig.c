#include "mp_trig.h"
#include <stddef.h>

#define MP_PI_OVER_180000 (3.14159265358979323846f / 180000.0f)

/* Reduce to [-90,90] degrees before evaluating an 11th-order polynomial.
 * Period normalization is done in int64 to avoid overflow for any int32 input.
 */
static float sine_mdeg(int64_t angle)
{
    int64_t reduced = angle % 360000LL;
    float x, x2;
    if (reduced > 180000LL) reduced -= 360000LL;
    if (reduced < -180000LL) reduced += 360000LL;
    if (reduced > 90000LL) reduced = 180000LL - reduced;
    if (reduced < -90000LL) reduced = -180000LL - reduced;

    x = (float)reduced * MP_PI_OVER_180000;
    x2 = x * x;
    return x * (1.0f + x2 * (-1.0f / 6.0f +
           x2 * (1.0f / 120.0f +
           x2 * (-1.0f / 5040.0f +
           x2 * (1.0f / 362880.0f +
           x2 * (-1.0f / 39916800.0f))))));
}

bool MP_TrigSinCosMdeg(int32_t angle_mdeg, float *sine, float *cosine)
{
    if (sine == NULL || cosine == NULL) return false;
    *sine = sine_mdeg((int64_t)angle_mdeg);
    *cosine = sine_mdeg((int64_t)angle_mdeg + 90000LL);
    return true;
}
