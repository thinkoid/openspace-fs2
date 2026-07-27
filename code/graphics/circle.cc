/*
 * Copyright (C) Volition, Inc. 1999.  All rights reserved.
 *
 * All source code herein is the property of Volition, Inc. You may not sell 
 * or otherwise commercially exploit the source or things you created based on the 
 * source.
 *
*/ 

#include <graphics/2d.hh>
#include <graphics/pixel.hh>
#include <graphics/circle.hh>

// THIS COULD BE OPTIMIZED BY MOVING THE GR_RECT CODE INTO HERE!!!!!!!!

#define circle_fill(x,y,w) gr_rect((x),(y),(w),1)

void gr8_circle( int xc, int yc, int d )
{
	int p,x, y, r;

	r = d/2;
	p=3-d;
	x=0;
	y=r;

	// Big clip
	if ( (xc+r) < gr_screen.clip_left ) return;
	if ( (xc-r) > gr_screen.clip_right ) return;
	if ( (yc+r) < gr_screen.clip_top ) return;
	if ( (yc-r) > gr_screen.clip_bottom ) return;

	while(x<y)	{
		// Draw the first octant
		circle_fill( xc-y, yc-x, y*2+1 );
		if ( x != 0 )	
			circle_fill( xc-y, yc+x, y*2+1 );

		if (p<0) 
			p=p+(x<<2)+6;
		else	{
			// Draw the second octant
			circle_fill( xc-x, yc-y, x*2+1 );
			if ( y != 0 )	
				circle_fill( xc-x, yc+y, x*2+1 );
			p=p+((x-y)<<2)+10;
			y--;
		}
		x++;
	}

	if(x==y)	{
		circle_fill( xc-x, yc-y, x*2+1 );
		if ( y != 0 )	
			circle_fill( xc-x, yc+y, x*2+1 );
	}

}

