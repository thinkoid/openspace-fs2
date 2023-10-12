// -*- mode: c++; -*-

#ifndef FREESPACE2_MATH_FIX_HH
#define FREESPACE2_MATH_FIX_HH

#include <defs.hh>

#define F1_0 65536
#define f1_0 65536

inline fix fixmul (fix a, fix b) {
    long result = long (a) * long (b);
    return static_cast< fix > (result >> 16);
}

constexpr int f2i (fix a) {
    return static_cast< int > (a >> 16);
}

constexpr fix i2f (int a) {
    return static_cast< fix > (a << 16);
}

#endif // FREESPACE2_MATH_FIX_HH
