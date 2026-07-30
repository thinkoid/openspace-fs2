/*
 * Copyright (C) Volition, Inc. 1999.  All rights reserved.
 *
 * All source code herein is the property of Volition, Inc. You may not sell 
 * or otherwise commercially exploit the source or things you created based on the 
 * source.
 *
*/

#include <stdlib.h>
#include <math.h>

#include <globalincs/pstypes.hh>
#include <math/floating.hh>

// rounds off a floating point number to a multiple of some number
float
fl_roundoff(float x, int multiple)
{
    float half = (float)multiple / 2.0f;

    if (x < 0)
        half = -half;

    x += half;
    return (float)(((int)x / multiple) * multiple);
}

// Return random value in range 0.0..1.0- (1.0- means the closest number less than 1.0)
float
frand()
{
    // retail divided by (RAND_MAX + 1) with MSVC's RAND_MAX of 0x7fff; glibc's
    // RAND_MAX is INT_MAX, so that sum overflows to INT_MIN and every frand()
    // came out negative.  Mask to the 15 bits retail was tuned against.
    float rval;
    rval = ((float)(myrand() & 0x7fff)) / (0x7fff + 1);
    return rval;
}

// Return a floating point number in the range min..max.
float
frand_range(float min, float max)
{
    float rval;

    rval = frand();
    rval = rval * (max - min) + min;

    return rval;
}

// Call this in the frame interval to get TRUE chance times per second.
// If you want it to return TRUE 3 times per second, call it in the frame interval like so:
//    rand_chance(flFrametime, 3.0f);
int
rand_chance(float frametime, float chance) //   default value for chance = 1.0f.
{
    while (--chance > 0.0f)
        if (frand() < frametime)
            return 1;

    return frand() < (frametime * (chance + 1.0f));
}

/*fix fl2f( float x )
{
   float nf;
   nf = x*65536.0f + 8390656.0f;
   return ((*((int *)&nf)) & 0x7FFFFF)-2048;
}
*/

/*
>#define  S  65536.0
>#define  MAGIC  (((S * S * 16) + (S*.5)) * S)
>
>#pragma inline float2int;
>
>ulong float2int( float d )
>{
>  double dtemp = MAGIC + d;
>  return (*(ulong *)&dtemp) - 0x80000000;
>}

*/
