/*
 * Copyright (C) Volition, Inc. 1999.  All rights reserved.
 *
 * All source code herein is the property of Volition, Inc. You may not sell 
 * or otherwise commercially exploit the source or things you created based on the 
 * source.
 *
*/ 

// (windows.h removed)
// (windowsx.h removed)

#include <graphics/2d.hh>
#include <graphics/grinternal.hh>
#include <math/floating.hh>
#include <graphics/line.hh>
#include <io/key.hh>


void gr8_uline(int x1,int y1,int x2,int y2)
{
	int i;
   int xstep,ystep;
   int dy=y2-y1;
   int dx=x2-x1;
   int error_term=0;

	gr_lock();
	ubyte *dptr = GR_SCREEN_PTR(ubyte,x1,y1);
	ubyte color = gr_screen.current_color.raw8;
		
	if(dy<0)	{
		dy=-dy;
      ystep=-gr_screen.rowsize / gr_screen.bytes_per_pixel;
	}	else	{
      ystep=gr_screen.rowsize / gr_screen.bytes_per_pixel;
	}

   if(dx<0)	{
      dx=-dx;
      xstep=-1;
   } else {
      xstep=1;
	}

	/* HARDWARE_ONLY - removed alpha color table stuff
	if ( Current_alphacolor )	{
		if(dx>dy)	{

			for(i=dx+1;i>0;i--) {
				*dptr = Current_alphacolor->table.lookup[14][*dptr];
				dptr += xstep;
				error_term+=dy;

				if(error_term>dx)	{
					error_term-=dx;
					dptr+=ystep;
				}
			}
		} else {

			for(i=dy+1;i>0;i--)	{
				*dptr = Current_alphacolor->table.lookup[14][*dptr];
				dptr += ystep;
				error_term+=dx;
				if(error_term>0)	{
					error_term-=dy;
					dptr+=xstep;
				}

			}

		}
	} else {
	*/
		if(dx>dy)	{

			for(i=dx+1;i>0;i--) {
				*dptr = color;
				dptr += xstep;
				error_term+=dy;

				if(error_term>dx)	{
					error_term-=dx;
					dptr+=ystep;
				}
			}
		} else {

			for(i=dy+1;i>0;i--)	{
				*dptr = color;
				dptr += ystep;
				error_term+=dx;
				if(error_term>0)	{
					error_term-=dy;
					dptr+=xstep;
				}

			}

		}	
	gr_unlock();
}  



void gr8_line(int x1,int y1,int x2,int y2)
{
	int clipped = 0, swapped=0;

	INT_CLIPLINE(x1,y1,x2,y2,gr_screen.clip_left,gr_screen.clip_top,gr_screen.clip_right,gr_screen.clip_bottom,return,clipped=1,swapped=1);

	gr8_uline(x1,y1,x2,y2);
}



