// -*- mode: c++; -*-

#ifndef FREESPACE2_MATH_FLOATING_HH
#define FREESPACE2_MATH_FLOATING_HH

#include <defs.hh>

#include <cmath>
#include <cmath>
#include <cfloat>

#define IS_NAN(x) (std::isnan (x))
#define IS_NEAR_ZERO(x, e) (std::fabs (x) < float (e))

// Handy macros to prevent type casting all over the place
#define fl2ir(fl) ((int)(fl + ((fl < 0.0f) ? -0.5f : 0.5f)))
#define f2fl(fx) ((float)(fx) / 65536.0f)
#define fl2f(fl) (int)((fl)*65536.0f)

// convert a measurement in degrees to radians
inline float to_radians (float arg) {
    return arg * (PI / 180.0f);
}

inline float to_degrees (float arg) {
    return arg * (180.0f / PI);
}

inline bool eqf (float a, float b) {
    return fabsf (a - b) <= FLT_EPSILON * MAX (
        1.0f, MAX (fabsf (a), fabsf (b)));
}

inline float mroundf (float x, int y) {
    return roundf (x / y) * y;
}

#endif // FREESPACE2_MATH_FLOATING_HH
