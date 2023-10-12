// -*- mode: c++; -*-

#ifndef FREESPACE2_MATH_STATICRAND_HH
#define FREESPACE2_MATH_STATICRAND_HH

#include <defs.hh>
#include <math/vecmat.hh>

namespace fs2 {
namespace prng {

namespace {

//
// George Marsaglia's PRNGs:
//
inline unsigned znew () {
    static unsigned z = 362436069U;
    return (z = 36969 * (z & 65535) + (z >> 16));
}

inline unsigned wnew () {
    static unsigned w = 521288629U;
    return (w = 18000 * (w & 65535) + (w >> 16));
}

inline unsigned MWC () {
    return ((znew () << 16) + wnew ());
}

inline unsigned SHR3 () {
    static unsigned jsr = 123456789U;
    return (jsr ^= (jsr << 17), jsr ^= (jsr >> 13), jsr ^= (jsr << 5));
}

inline unsigned CONG () {
    static unsigned jcong = 380116160U;
    return (jcong = 69069 * jcong + 1234567);
}

inline unsigned FIB () {
    static unsigned a = 224466889U, b = 7584631U;
    return ((b = a + b), (a  =b - a));
}

inline unsigned KISS () {
    return (MWC () ^ CONG ()) + SHR3 ();
}

} // anonymous

inline int randi (int) {
    return KISS ();
}

inline int randi (int, int lo, int hi) {
    return KISS () % (hi - lo) + lo;
}

inline float randf (int) {
    return (KISS () & 0xFFFF) / 65536.f;
}

inline float randf (int, float lo, float hi) {
    const auto x = randf (0);
    return x * (hi - lo) + lo;
}

inline vec3d rand3f (int) {
    vec3d x = {{ randf (0) - .5f, randf (0) - .5f, randf (0) - .5f }};
    vm_vec_normalize_quick (&x);
    return x;
}

} // namespace prng
} // namespace fs2

#endif // FREESPACE2_MATH_STATICRAND_HH
