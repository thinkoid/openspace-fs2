/*
 * Copyright (C) Volition, Inc. 1999.  All rights reserved.
 *
 * All source code herein is the property of Volition, Inc. You may not sell 
 * or otherwise commercially exploit the source or things you created based on the 
 * source.
 *
*/

#ifndef _FLOATING_H
#define _FLOATING_H

#include <math.h>
#include <float.h>

extern float frand();
extern int rand_chance(float frametime, float chance = 1.0f);
float frand_range(float min, float max);

// Handy macros to prevent type casting all over the place

#define fl_sqrt(fl) (float)sqrt((float)(fl))
#define fl_isqrt(fl) (1.0f / (float)sqrt((float)(fl)))
#define fl_abs(fl) (float)fabs((double)(fl))
#define i2fl(i) ((float)(i))
#define fl2i(fl) ((int)(fl))
#define f2fl(fx) ((float)(fx) / 65536.0f)
#define fl2f(fl) (int)((fl) * 65536.0f)

// convert a measurement in degrees to radians
#define fl_radian(fl) ((float)((fl * 3.14159f) / 180.0f))

// use this instead of:
// for:  (int)floor(x+0.5f) use fl_round_2048(x)
//       (int)ceil(x-0.5f)  use fl_round_2048(x)
//       (int)floor(x-0.5f) use fl_round_2048(x-1.0f)
//       (int)floor(x)      use fl_round_2048(x-0.5f)
// for values in the range -2048 to 2048
// use this instead of:
// for:  (int)floor(x+0.5f) use fl_round_2048(x)
//       (int)ceil(x-0.5f)  use fl_round_2048(x)
//       (int)floor(x-0.5f) use fl_round_2048(x-1.0f)
//       (int)floor(x)      use fl_round_2048(x-0.5f)
// for values in the range -2048 to 2048

// Retail did this with the 2^52+2^51 magic-add trick, which depended on
// x87 extended-precision float arithmetic: under SSE2 the float add wipes
// out x and the low word reads back 0 for every input.  lrintf() is the
// same round-to-nearest-even the x87 trick computed.
inline int
fl_round_2048(float x)
{
    return (int)lrintf(x);
}

/*
inline float fl_sqrt( float x)
{
   float retval;

   _asm fld x
   _asm fsqrt
   _asm fstp retval
   
   return retval;
}

float fl_isqrt( float x )
{
   float retval;

   _asm fld x
   _asm fsqrt
   _asm fstp retval
   
   return 1.0f / retval;
} 
*/

// rounds off a floating point number to a multiple of some number
extern float fl_roundoff(float x, int multiple);

#endif
