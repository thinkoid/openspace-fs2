/*
 * Copyright (C) Volition, Inc. 1999.  All rights reserved.
 *
 * All source code herein is the property of Volition, Inc. You may not sell 
 * or otherwise commercially exploit the source or things you created based on the 
 * source.
 *
*/ 

#include <math.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

#include "scaler.h"
#include "2d.h"
#include "grinternal.h"
#include "floating.h"
#include "bmpman.h"
#include "palman.h"
#include "tmapscanline.h"
#include "systemvars.h"
#include "key.h"
#include "colors.h"

#define MIN_SCALE_FACTOR 0.0001f

#define USE_COMPILED_CODE

#define TRANSPARENCY_COLOR_8		0xff
#define TRANSPARENCY_COLOR_16		0xffff
#define TRANSPARENCY_COLOR_32		0xffffffff

#define FIND_SCALED_NUM(x,x0,x1,y0,y1) (((((x)-(x0))*((y1)-(y0)))/((x1)-(x0)))+(y0))

// The original renderer generated x86 machine code on the fly into a
// compiled_code[] buffer (one routine per span shape, with w/u/du baked
// into the instruction stream) and called it with esi=sbits, edi=dbits,
// ecx=lookup, edx=zbuf, ebp=Gr_global_z.  The scaler_span8* functions
// below are straight C equivalents of the generated code; the register
// roles survive as parameter names.

typedef void (*scaler_span_fn)( ubyte *sbits, ubyte *dbits, ubyte *lookup, uint *zbuf, uint gz, int w, fix u, fix du );

/*
void test_code()
{
	_asm mov ax, [esi+0xabcdef12]
	_asm cmp ax, 255
	_asm je  0xabcdef12
	_asm mov [edi+0xabcdef12], ax
	_asm mov ax, [esi+0xabcdef12]
}
*/



//----------------------------------------------------
// scaler_span8  (was scaler_create_compiled_code8)
//
// Created code that looked like:
//
// @@: mov al, [esi+????]
//     cmp al, TRANSPARENCY_COLOR_8
//     je  @f   ; jump to next @@ label
//     mov [edi+???], al    ; If the source pixel is scaled up
//     mov [edi+???], al    ; there might be a lot of these lines
//     ...
// @@: mov al, [esi+????]
//
// The generated code read the texel and tested transparency once per
// run of identical f2i(u), skipping all the writes of a transparent
// run; re-reading per pixel below gives identical results.

static void scaler_span8( ubyte *sbits, ubyte *dbits, ubyte *lookup, uint *zbuf, uint gz, int w, fix u, fix du )
{
	int x;

	for (x=0; x<w; x++ )			{
		ubyte al = sbits[ f2i(u) ];				// mov al, [esi+f2i(u)]
		if ( al != TRANSPARENCY_COLOR_8 )	{	// cmp al, 255 / je @f
			dbits[x] = al;						// mov [edi+x], al
		}
		u += du;
	}
}

// scaler_span8_stippled  (was scaler_create_compiled_code8_stippled)
// Same as scaler_span8 but writes every other pixel, stepping u twice.

static void scaler_span8_stippled( ubyte *sbits, ubyte *dbits, ubyte *lookup, uint *zbuf, uint gz, int w, fix u, fix du )
{
	int x;

	for (x=0; x<w-1; x+=2 )			{
		ubyte al = sbits[ f2i(u) ];				// mov al, [esi+f2i(u)]
		if ( al != TRANSPARENCY_COLOR_8 )	{	// cmp al, 255 / je @f
			dbits[x] = al;						// mov [edi+x], al
		}
		u += du*2;
	}
}

/*
void test_code1()
{
	_asm mov ebx, -1
	_asm xor eax, eax
	_asm xor ebx, ebx
	_asm mov	bl, BYTE PTR [edi-1412567278]
	_asm add ebx, eax
	_asm mov ebx, [ecx+ebx]	; blend it
	_asm cmp ebp, [edx]
	_asm add edx, 4
	_asm jl [0xABCDEF12]

//     xor eax, eax			; avoid ppro partial register stall
//     mov ah, [esi+????]   ; get the foreground pixel
//     ; the following lines might be repeated
//     xor ebx, ebx			; avoid ppro partial register stall
//     mov bl, [edi+????]   ; get the background pixel
//     mov ebx, [ecx+ebx]	; blend it
//     mov [edi+????], bl   ; write it
}
*/

/*
  00130	b8 00 00 00 00	mov	eax, 0
  00135	8a a6 12 ef cd ab		mov	ah, BYTE PTR [esi-1412567278]
  0013b	8a 87 12 ef cd ab		mov	al, BYTE PTR [edi-1412567278]
  00141	8a 1c 01	            mov	bl, BYTE PTR [ecx+eax]
  00141	8b 1c 01					mov	ebx, DWORD PTR [ecx+eax]
  00144	88 9f 12 ef cd ab		mov	BYTE PTR [edi-1412567278], bl


  00130	33 c0		xor	eax, eax
  00132	33 db		xor	ebx, ebx
  00134	8a 9f 12 ef cd	ab		mov	bl, BYTE PTR [edi-1412567278]
  0013a	03 d8		add	ebx, eax
  0013c	8b 1c 19	mov	ebx, DWORD PTR [ecx+ebx]

  0013f	3b 2a		cmp	ebp, DWORD PTR [edx]
  00141	83 c2 04	add	edx, 4


*/

//----------------------------------------------------
// scaler_span8_alpha  (was scaler_create_compiled_code8_alpha)
//
// Created code that looked like:

//=============== Pentium ======================
// mov eax, 0
//     mov ah, [esi+????]   ; get the foreground pixel
//     ; the following lines might be repeated
//     mov al, [edi+????]   ; get the background pixel
//     mov bl, [ecx+eax]	; blend it
//     mov [edi+????], bl   ; write it
//     ...

//============= Pentium Pro code =============
//     xor eax, eax			; avoid ppro partial register stall
//     mov ah, [esi+????]   ; get the foreground pixel
//     ; the following lines might be repeated
//     xor ebx, ebx			; avoid ppro partial register stall
//     mov bl, [edi+????]   ; get the background pixel
//     mov ebx, [ecx+ebx]	; blend it
//     mov [edi+????], bl   ; write it


// Both CPU variants blended through the same table:
//   Pentium:     eax = (texel<<8) | background, bl = byte [ecx+eax]
//   Pentium Pro: ebx = background + (texel<<8), ebx = dword [ecx+ebx]
// then wrote bl, so both come down to
//   dbits[x] = lookup[ (sbits[f2i(u)]<<8) + dbits[x] ]
// (the PPro variant's dword load only ever had its low byte used).
// No transparency test: index 255 goes through the blend table too.

static void scaler_span8_alpha( ubyte *sbits, ubyte *dbits, ubyte *lookup, uint *zbuf, uint gz, int w, fix u, fix du )
{
	int x;

	for (x=0; x<w; x++ )			{
		uint eax = ((uint)sbits[ f2i(u) ]) << 8;	// mov ah, [esi+f2i(u)]
		dbits[x] = lookup[ eax + dbits[x] ];		// mov bl, [ecx+eax+bg] / mov [edi+x], bl
		u += du;
	}
}

/*
				for (x=0; x<w; x++ )			{
					if ( fx_w > *zbuf )	{
						uint c = sbits[ tmp_u >> 16 ]<<8;
						*dbits = *((ubyte *)(lookup + (*dbits | c)));
					}
					dbits++;
					zbuf++;
					tmp_u += du;
				}
*/

//----------------------------------------------------
// scaler_span8_alpha_zbuffered  (was scaler_create_compiled_code8_alpha_zbuffered)
//
// Created code that looked like:
// mov eax, 0
//     mov ah, [esi+????]   ; get the foreground pixel
//     ; the following lines might be repeated
//     cmp	fx_w, [edx+?????]
//     jle  @f
//     mov al, [edi+????]   ; get the background pixel
//     mov bl, [ecx+eax]	; blend it
//     mov [edi+????], bl   ; write it
//  @@:
//     ...




//void test_code1()
//{
//	_asm cmp 0xFFFFFFFF, [edx+0xabcdef12]
//	_asm cmp ebp, [edx+0xabcdef12]
//	_asm jle	0xabcdef12
//}
//; 302  : 	_asm cmp ebp, [edx+0xabcdef12]
//  00244	3b aa 12 ef cd ab		cmp	ebp, DWORD PTR [edx-1412567278]
//; 303  : 	_asm jle	0xabcdef12
//  0024a	0f 8e 12 ef cd ab		jle	-1412567278		; abcdef12H

// Same blend as scaler_span8_alpha, gated per pixel on the z test
// "cmp ebp, [edx+x*4] / jle skip":  draw when gz > zbuf[x], as a
// SIGNED 32-bit compare (jle), even though both values are uints.

static void scaler_span8_alpha_zbuffered( ubyte *sbits, ubyte *dbits, ubyte *lookup, uint *zbuf, uint gz, int w, fix u, fix du )
{
	int x;

	for (x=0; x<w; x++ )			{
		if ( (int)gz > (int)zbuf[x] )	{				// cmp ebp, [edx+x*4] / jle @f
			uint eax = ((uint)sbits[ f2i(u) ]) << 8;	// mov ah, [esi+f2i(u)]
			dbits[x] = lookup[ eax + dbits[x] ];		// mov bl, [ecx+eax+bg] / mov [edi+x], bl
		}
		u += du;
	}
}



int Gr_scaler_zbuffering = 0;
uint Gr_global_z;

MONITOR( ScalerNumCalls );	


//----------------------------------------------------
// Scales current bitmap, between va and vb
void gr8_scaler(vertex *va, vertex *vb )
{
#if 1
	if(Pofview_running){
		return;
	}

	float x0, y0, x1, y1;
	float u0, v0, u1, v1;
	float clipped_x0, clipped_y0, clipped_x1, clipped_y1;
	float clipped_u0, clipped_v0, clipped_u1, clipped_v1;
	float xmin, xmax, ymin, ymax;
	int dx0, dy0, dx1, dy1;

	MONITOR_INC( ScalerNumCalls, 1 );	

	//============= CLIP IT =====================

	x0 = va->sx; y0 = va->sy;
	x1 = vb->sx; y1 = vb->sy;

	xmin = i2fl(gr_screen.clip_left); ymin = i2fl(gr_screen.clip_top);
	xmax = i2fl(gr_screen.clip_right); ymax = i2fl(gr_screen.clip_bottom);

	u0 = va->u; v0 = va->v;
	u1 = vb->u; v1 = vb->v;

	// Check for obviously offscreen bitmaps...
	if ( (y1<=y0) || (x1<=x0) ) return;
	if ( (x1<xmin ) || (x0>xmax) ) return;
	if ( (y1<ymin ) || (y0>ymax) ) return;

	clipped_u0 = u0; clipped_v0 = v0;
	clipped_u1 = u1; clipped_v1 = v1;

	clipped_x0 = x0; clipped_y0 = y0;
	clipped_x1 = x1; clipped_y1 = y1;

	// Clip the left, moving u0 right as necessary
	if ( x0 < xmin ) 	{
		clipped_u0 = FIND_SCALED_NUM(xmin,x0,x1,u0,u1);
		clipped_x0 = xmin;
	}

	// Clip the right, moving u1 left as necessary
	if ( x1 > xmax )	{
		clipped_u1 = FIND_SCALED_NUM(xmax,x0,x1,u0,u1);
		clipped_x1 = xmax;
	}

	// Clip the top, moving v0 down as necessary
	if ( y0 < ymin ) 	{
		clipped_v0 = FIND_SCALED_NUM(ymin,y0,y1,v0,v1);
		clipped_y0 = ymin;
	}

	// Clip the bottom, moving v1 up as necessary
	if ( y1 > ymax ) 	{
		clipped_v1 = FIND_SCALED_NUM(ymax,y0,y1,v0,v1);
		clipped_y1 = ymax;
	}
	
	dx0 = fl2i(clipped_x0); dx1 = fl2i(clipped_x1);
	dy0 = fl2i(clipped_y0); dy1 = fl2i(clipped_y1);

	if (dx1<=dx0) return;
	if (dy1<=dy0) return;

	//============= DRAW IT =====================
	int u, v, du, dv;
	int y, w;
	ubyte * sbits, * dbits;
	bitmap * bp;
	ubyte * spixels;
	float tmpu, tmpv;

	tmpu = (clipped_u1-clipped_u0) / (dx1-dx0);
	if ( fl_abs(tmpu) < MIN_SCALE_FACTOR ) {
		return;		// scaled up way too far!
	}
	tmpv = (clipped_v1-clipped_v0) / (dy1-dy0);
	if ( fl_abs(tmpv) < MIN_SCALE_FACTOR ) {
		return;		// scaled up way too far!
	}

	int is_stippled = 0;

	/*
	if ( !Detail.alpha_effects )	{
		is_stippled = 1;
		Gr_scaler_zbuffering = 0;
	}
	*/
	
	if ( is_stippled )	{
		bp = bm_lock( gr_screen.current_bitmap, 8, 0 );
	} else {
		bp = bm_lock( gr_screen.current_bitmap, 8, 0 );
	}


	du = fl2f(tmpu*(bp->w-1));
	dv = fl2f(tmpv*(bp->h-1));

	v = fl2f(clipped_v0*(bp->h-1));
	u = fl2f(clipped_u0*(bp->w-1)); 
	w = dx1 - dx0 + 1;
	if ( w < 2 ) {
		bm_unlock(gr_screen.current_bitmap);
		return;
	}

	uint fx_w = 0;
	if ( Gr_scaler_zbuffering && gr_zbuffering )	{
		fx_w = (uint)fl2i(va->sw * GR_Z_RANGE)+gr_zoffset;
		Gr_global_z = fx_w;
	}

#ifdef USE_COMPILED_CODE
	scaler_span_fn cc=NULL;

	if ( Gr_scaler_zbuffering && gr_zbuffering )	{
		if ( gr_screen.current_alphablend_mode == GR_ALPHABLEND_FILTER )	{
			cc = scaler_span8_alpha_zbuffered;
		}
	} else {
		if ( gr_screen.current_alphablend_mode == GR_ALPHABLEND_FILTER )	{
			if ( is_stippled )	{
				cc = scaler_span8_stippled;
			} else {
				cc = scaler_span8_alpha;
			}
		} else	{
			cc = scaler_span8;
		}
	}

#endif

	spixels = (ubyte *)bp->data;

	gr_lock();

	uint *zbuf = NULL;

	for (y=dy0; y<=dy1; v += dv, y++ )			{
		if ( is_stippled && (y&1) )	{
			sbits = &spixels[bp->rowsize*(v>>16)+f2i(du)];
			dbits = GR_SCREEN_PTR(ubyte,dx0+1,y);
		} else {
			sbits = &spixels[bp->rowsize*(v>>16)];
			dbits = GR_SCREEN_PTR(ubyte,dx0,y);
		}
		ubyte *lookup = NULL;

		if ( gr_screen.current_alphablend_mode == GR_ALPHABLEND_FILTER )	{
			lookup = palette_get_blend_table(gr_screen.current_alpha);
		}

		if ( Gr_scaler_zbuffering && gr_zbuffering )	{
			zbuf = &gr_zbuffer[dbits-(ubyte *)gr_screen.offscreen_buffer_base];
		}
	
#ifdef USE_COMPILED_CODE
		// Call the compiled code to draw one scanline
		if ( Gr_scaler_zbuffering &&  gr_zbuffering && (gr_screen.current_alphablend_mode != GR_ALPHABLEND_FILTER))	{			
			Int3();

			/*
			int x, tmp_u;
			tmp_u = u;

			for (x=0; x<w; x++ )			{
				if ( fx_w > *zbuf )	{
					ubyte c = sbits[ tmp_u >> 16 ];
					if ( c != TRANSPARENCY_COLOR_8 ) *dbits = c;
				}
				zbuf++;
				dbits++;
				tmp_u += du;
			}
			*/
		} else {
/*			{
				int x, tmp_u;
				tmp_u = u;

	
				for (x=0; x<w; x++ )			{
					if ( fx_w > *zbuf )	{
						uint c = sbits[ tmp_u >> 16 ]<<8;
						*dbits = *((ubyte *)(lookup + (*dbits | c)));
					}
					dbits++;
					zbuf++;
					tmp_u += du;
				}
			} 
*/
			// was: ecx=lookup, esi=sbits, edi=dbits, edx=zbuf,
			// ebp=Gr_global_z, call the compiled span
			(*cc)( sbits, dbits, lookup, zbuf, Gr_global_z, w, u, du );
		}
#else	
		if ( gr_screen.current_alphablend_mode == GR_ALPHABLEND_FILTER )	{
			if ( Gr_scaler_zbuffering && gr_zbuffering )	{
				int x, tmp_u;
				tmp_u = u;

				for (x=0; x<w; x++ )			{
					if ( fx_w > *zbuf )	{
						uint c = sbits[ tmp_u >> 16 ]<<8;
						*dbits = *((ubyte *)(lookup + (*dbits | c)));
					}
					dbits++;
					zbuf++;
					tmp_u += du;
				}
			} else {
				int x, tmp_u;
				tmp_u = u;
				for (x=0; x<w; x++ )			{
					uint c = sbits[ tmp_u >> 16 ]<<8;
					*dbits++ = palette_blend[*dbits|c];
					tmp_u += du;
				}
			}
		} else {
			if ( Gr_scaler_zbuffering && gr_zbuffering )	{
				int x, tmp_u;
				tmp_u = u;
			
				for (x=0; x<w; x++ )			{
					if ( fx_w > *zbuf )	{
						ubyte c = sbits[ tmp_u >> 16 ];
						if ( c != TRANSPARENCY_COLOR_8 ) *dbits = c;
					}
					zbuf++;
					dbits++;
					tmp_u += du;
				}
			} else {
				int x, tmp_u;
				tmp_u = u;
				for (x=0; x<w; x++ )			{
					ubyte c = sbits[ tmp_u >> 16 ];
					if ( c != TRANSPARENCY_COLOR_8 ) *dbits = c;
					dbits++;
					tmp_u += du;
				}
			}
		}
#endif
	}

	gr_unlock();
	bm_unlock(gr_screen.current_bitmap);
#endif
}

int aiee = 0;
alphacolor_old old_alphac;
//----------------------------------------------------
// Scales current bitmap, between va and vb
void gr8_aascaler(vertex *va, vertex *vb )
{
	float x0, y0, x1, y1;
	float u0, v0, u1, v1;
	float clipped_x0, clipped_y0, clipped_x1, clipped_y1;
	float clipped_u0, clipped_v0, clipped_u1, clipped_v1;
	float xmin, xmax, ymin, ymax;
	int dx0, dy0, dx1, dy1;

	//if ( !Current_alphacolor )	return;

	MONITOR_INC( ScalerNumCalls, 1 );	

	Assert(Fred_running);
	if(!aiee){
		old_alphac.used = 1;
		old_alphac.r = 93;
		old_alphac.g = 93;
		old_alphac.b = 128;
		old_alphac.alpha = 255;
		//ac->type = type;
		//ac->clr=clr;
		//93, 93, 128, 255
		calc_alphacolor_old(&old_alphac);
		aiee = 1;
	}

	//============= CLIP IT =====================

	x0 = va->sx; y0 = va->sy;
	x1 = vb->sx; y1 = vb->sy;

	xmin = i2fl(gr_screen.clip_left); ymin = i2fl(gr_screen.clip_top);
	xmax = i2fl(gr_screen.clip_right); ymax = i2fl(gr_screen.clip_bottom);

	u0 = va->u; v0 = va->v;
	u1 = vb->u; v1 = vb->v;

	// Check for obviously offscreen bitmaps...
	if ( (y1<=y0) || (x1<=x0) ) return;
	if ( (x1<xmin ) || (x0>xmax) ) return;
	if ( (y1<ymin ) || (y0>ymax) ) return;

	clipped_u0 = u0; clipped_v0 = v0;
	clipped_u1 = u1; clipped_v1 = v1;

	clipped_x0 = x0; clipped_y0 = y0;
	clipped_x1 = x1; clipped_y1 = y1;

	// Clip the left, moving u0 right as necessary
	if ( x0 < xmin ) 	{
		clipped_u0 = FIND_SCALED_NUM(xmin,x0,x1,u0,u1);
		clipped_x0 = xmin;
	}

	// Clip the right, moving u1 left as necessary
	if ( x1 > xmax )	{
		clipped_u1 = FIND_SCALED_NUM(xmax,x0,x1,u0,u1);
		clipped_x1 = xmax;
	}

	// Clip the top, moving v0 down as necessary
	if ( y0 < ymin ) 	{
		clipped_v0 = FIND_SCALED_NUM(ymin,y0,y1,v0,v1);
		clipped_y0 = ymin;
	}

	// Clip the bottom, moving v1 up as necessary
	if ( y1 > ymax ) 	{
		clipped_v1 = FIND_SCALED_NUM(ymax,y0,y1,v0,v1);
		clipped_y1 = ymax;
	}
	
	dx0 = fl2i(clipped_x0); dx1 = fl2i(clipped_x1);
	dy0 = fl2i(clipped_y0); dy1 = fl2i(clipped_y1);

	if (dx1<=dx0) return;
	if (dy1<=dy0) return;

	//============= DRAW IT =====================
	int u, v, du, dv;
	int y, w;
	ubyte * sbits, * dbits;
	bitmap * bp;
	ubyte * spixels;
	float tmpu, tmpv;

	tmpu = (clipped_u1-clipped_u0) / (dx1-dx0);
	if ( fl_abs(tmpu) < MIN_SCALE_FACTOR ) {
		return;		// scaled up way too far!
	}
	tmpv = (clipped_v1-clipped_v0) / (dy1-dy0);
	if ( fl_abs(tmpv) < MIN_SCALE_FACTOR ) {
		return;		// scaled up way too far!
	}

	bp = bm_lock( gr_screen.current_bitmap, 8, BMP_AABITMAP );

	du = fl2f(tmpu*(bp->w-1));
	dv = fl2f(tmpv*(bp->h-1));

	v = fl2f(clipped_v0*(bp->h-1));
	u = fl2f(clipped_u0*(bp->w-1)); 
	w = dx1 - dx0 + 1;

#ifdef USE_COMPILED_CODE
	scaler_span_fn cc = NULL;

	if ( Gr_scaler_zbuffering && gr_zbuffering )	{
		//cc = scaler_span8_alpha_zbuffered;
	} else {
		cc = scaler_span8_alpha;
	}

#endif

	spixels = (ubyte *)bp->data;

	gr_lock();

	uint fx_w = 0;
	if ( Gr_scaler_zbuffering  && gr_zbuffering )	{
		fx_w = (uint)fl2i(va->sw * GR_Z_RANGE)+gr_zoffset;
	}	

	for (y=dy0; y<=dy1; y++ )			{
		sbits = &spixels[bp->rowsize*(v>>16)];
		dbits = GR_SCREEN_PTR(ubyte,dx0,y);
		// ubyte *lookup = &Current_alphacolor->table.lookup[0][0];
		ubyte *lookup = &old_alphac.table.lookup[0][0];
		
#ifdef USE_COMPILED_CODE
		// Call the compiled code to draw one scanline
		if ( Gr_scaler_zbuffering  && gr_zbuffering )	{
			int x, tmp_u;
			tmp_u = u;

			uint *zbuf = &gr_zbuffer[dbits-(ubyte *)gr_screen.offscreen_buffer_base];

			for (x=0; x<w; x++ )			{
				if ( fx_w > *zbuf )	{
					// uint c = sbits[ tmp_u >> 16 ];
					// *dbits = Current_alphacolor->table.lookup[c][*dbits];
					*dbits = (ubyte)0x00;
				}
				zbuf++;
				dbits++;
				tmp_u += du;
			}
		} else {
			// was: ecx=lookup, esi=sbits, edi=dbits, call the
			// compiled span (edx/ebp were not loaded here)
			(*cc)( sbits, dbits, lookup, NULL, 0, w, u, du );
		}
#else	
		if ( Gr_scaler_zbuffering && gr_zbuffering )	{
			int x, tmp_u;
			tmp_u = u;

			uint *zbuf = (uint *)&gr_zbuffer[(uint)dbits-(uint)Tmap.pScreenBits];
	
			for (x=0; x<w; x++ )			{
				if ( fx_w > *zbuf )	{
					uint c = sbits[ tmp_u >> 16 ];
					*dbits = Current_alphacolor->table.lookup[c][*dbits];
				}
				zbuf++;
				dbits++;
				tmp_u += du;
			}
		} else {
			int x, tmp_u;
			tmp_u = u;
			for (x=0; x<w; x++ )			{
				uint c = sbits[ tmp_u >> 16 ];
				*dbits = Current_alphacolor->table.lookup[c][*dbits];
				dbits++;
				tmp_u += du;
			}
		}
#endif
		v += dv;
	}

	gr_unlock();

	bm_unlock(gr_screen.current_bitmap);
}

