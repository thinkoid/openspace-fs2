/*
 * Copyright (C) Volition, Inc. 1999.  All rights reserved.
 *
 * All source code herein is the property of Volition, Inc. You may not sell 
 * or otherwise commercially exploit the source or things you created based on the 
 * source.
 *
*/ 

#include "pstypes.h"
#include "2d.h"
#include "grinternal.h"
#include "colors.h"
#include "palman.h"
#include "cfile.h"
#include "systemvars.h"

//#define MAX_ALPHACOLORS 36
#define MAX_ALPHACOLORS 72

alphacolor Alphacolors[MAX_ALPHACOLORS];
static int Alphacolors_intited = 0;

alphacolor * Current_alphacolor = NULL;


void calc_alphacolor_hud_type( alphacolor * ac )
{
#ifndef HARDWARE_ONLY
	int i,j;
	int tr,tg,tb, Sr, Sg, Sb;
	ubyte * pal;
	int r, g, b, alpha;
	float falpha;

	Assert(Alphacolors_intited);

//	mprintf(( "Calculating alphacolor for %d,%d,%d,%d\n", ac->r, ac->g, ac->b, ac->alpha ));

	falpha = i2fl(ac->alpha)/255.0f;
	if ( falpha<0.0f ) falpha = 0.0f; else if ( falpha > 1.0f ) falpha = 1.0f;

	alpha = ac->alpha >> 4;
	if (alpha < 0 ) alpha = 0; else if (alpha > 15 ) alpha = 15;
	r = ac->r;
	if (r < 0 ) r = 0; else if (r > 255 ) r = 255;
	g = ac->g;
	if (g < 0 ) g = 0; else if (g > 255 ) g = 255;
	b = ac->b;
	if (b < 0 ) b = 0; else if (b > 255 ) b = 255;

	int ii[16];

	for (j=1; j<15; j++ )	{

		// JAS: Use 1.5/Gamma instead of 1/Gamma because on Adam's
		// PC a gamma of 1.2 makes text look good, but his gamma is
		// really 1.8.   1.8/1.2 = 1.5
		float factor = falpha * (float)pow(i2fl(j)/14.0f, 1.5f/Gr_gamma);
		//float factor = i2fl(j)/14.0f;

		tr = fl2i( i2fl(r) * factor );
		tg = fl2i( i2fl(g) * factor );
		tb = fl2i( i2fl(b) * factor );

		ii[j] = tr;
		if ( tg > ii[j] )	ii[j] = tg;
		if ( tb > ii[j] )	ii[j] = tb;
	}

	pal = gr_palette;

	int m = r;
	if ( g > m ) m = g;
	if ( b > m ) m = b;

	ubyte ri[256], gi[256], bi[256];

	if ( m > 0 )	{
		for (i=0; i<256; i++ )	{
			ri[i] = ubyte((i*r)/m);
			gi[i] = ubyte((i*g)/m);
			bi[i] = ubyte((i*b)/m);
		}
	} else {
		for (i=0; i<256; i++ )	{
			ri[i] = 0;
			gi[i] = 0;
			bi[i] = 0;
		}
	}

	for (i=0; i<256; i++ )	{
		Sr = pal[0];
		Sg = pal[1];
		Sb = pal[2];
		pal += 3;

		int dst_intensity = Sr;
		if ( Sg > dst_intensity ) dst_intensity = Sg;
		if ( Sb > dst_intensity ) dst_intensity = Sb;

		ac->table.lookup[0][i] = (unsigned char)i;

		for (j=1; j<15; j++ )	{

			int tmp_i = max( ii[j], dst_intensity );

			ac->table.lookup[j][i] = (unsigned char)palette_find(ri[tmp_i],gi[tmp_i],bi[tmp_i]);
		}

		float di = (i2fl(Sr)*.30f+i2fl(Sg)*0.60f+i2fl(Sb)*.10f)/255.0f;
		float factor = 0.0f + di*0.75f;

		tr = fl2i( factor*i2fl(r)*falpha );
		tg = fl2i( factor*i2fl(g)*falpha );
		tb = fl2i( factor*i2fl(b)*falpha );

		if ( tr > 255 ) tr = 255; else if ( tr < 0 ) tr = 0;
		if ( tg > 255 ) tg = 255; else if ( tg < 0 ) tg = 0; 
		if ( tb > 255 ) tb = 255; else if ( tb < 0 ) tb = 0;

		ac->table.lookup[15][i] = (unsigned char)palette_find(tr,tg,tb);
		//ac->table.lookup[15][i] = (unsigned char)palette_find(255,0,0);

	}
#endif
}

// Old way to calculate alpha colors

void calc_alphacolor_blend_type( alphacolor * ac )
{
#ifndef HARDWARE_ONLY
	int i,j;
	int tr,tg,tb, Sr, Sg, Sb;
	int Dr, Dg, Db;
	ubyte * pal;
	int r, g, b, alpha;

	Assert(Alphacolors_intited);

//	mprintf(( "Calculating alphacolor for %d,%d,%d,%d\n", ac->r, ac->g, ac->b, ac->alpha ));

	alpha = ac->alpha >> 4;
	if (alpha < 0 ) alpha = 0; else if (alpha > 15 ) alpha = 15;
	r = ac->r;
	if (r < 0 ) r = 0; else if (r > 255 ) r = 255;
	g = ac->g;
	if (g < 0 ) g = 0; else if (g > 255 ) g = 255;
	b = ac->b;
	if (b < 0 ) b = 0; else if (b > 255 ) b = 255;

	int gamma_j1[16];

	for (j=1; j<16; j++ )	{
		// JAS: Use 1.5/Gamma instead of 1/Gamma because on Adam's
		// PC a gamma of 1.2 makes text look good, but his gamma is
		// really 1.8.   1.8/1.2 = 1.5
		gamma_j1[j] = (int)((pow(i2fl(j)/15.0f, 1.5f/Gr_gamma)*16.0f) + 0.5);
	}

	pal = gr_palette;

	for (i=0; i<256; i++ )	{
		Sr = pal[0];
		Sg = pal[1];
		Sb = pal[2];
		pal += 3;
	
		Dr = ( Sr*(16-alpha) + (r*alpha) ) >> 4;
		Dg = ( Sg*(16-alpha) + (g*alpha) ) >> 4;
		Db = ( Sb*(16-alpha) + (b*alpha) ) >> 4;

		ac->table.lookup[0][i] = (unsigned char)i;

		for (j=1; j<16; j++ )	{

			int j1 = gamma_j1[j];

			tr = ( Sr*(16-j1) + (Dr*j1) ) >> 4;
			tg = ( Sg*(16-j1) + (Dg*j1) ) >> 4;
			tb = ( Sb*(16-j1) + (Db*j1) ) >> 4;

			if ( tr > 255 ) tr = 255; else if ( tr < 0 ) tr = 0;
			if ( tg > 255 ) tg = 255; else if ( tg < 0 ) tg = 0; 
			if ( tb > 255 ) tb = 255; else if ( tb < 0 ) tb = 0;

			ac->table.lookup[j][i] = (unsigned char)palette_find(tr,tg,tb);
		}
	}
#endif
}

void calc_alphacolor( alphacolor * ac )
{
	switch(ac->type)	{
	case AC_TYPE_HUD:
		calc_alphacolor_hud_type(ac);
		break;
	case AC_TYPE_BLEND:
		calc_alphacolor_blend_type(ac);
		break;
	default:
		Int3();		// Passing an invalid type of alphacolor!
	}
}

void grx_init_alphacolors()
{
	int i;
	
	Alphacolors_intited = 1;

	for (i=0; i<MAX_ALPHACOLORS; i++ )	{
		Alphacolors[i].used = 0;
		Alphacolors[i].clr = NULL;
	}

	Current_alphacolor = NULL;
}



void grx_init_alphacolor( color *clr, int r, int g, int b, int alpha, int type )
{
	int n;
	alphacolor *ac;
	
	if(Game_mode & GM_STANDALONE_SERVER){
		return;
	}

	if (!Alphacolors_intited) return;

	if ( alpha < 0 ) alpha = 0;
	if ( alpha > 255 ) alpha = 255;

	n = -1;
	if ( (clr->magic == 0xAC01) && (clr->is_alphacolor) )	{
		if ( (clr->alphacolor >= 0) && (clr->alphacolor < MAX_ALPHACOLORS))	{
			if ( Alphacolors[clr->alphacolor].used && (Alphacolors[clr->alphacolor].clr==clr) )	{
				n = clr->alphacolor;
			}
		}
	}

	int changed = 0;

	if ( n==-1 )	{
		for (n=0; n<MAX_ALPHACOLORS; n++ )	{
			if (!Alphacolors[n].used) break;
		}
		if ( n == MAX_ALPHACOLORS )	
			Error( LOCATION, "Out of alphacolors!\n" );
	} else {
		changed = 1;
	}


	// Create the alphacolor
	ac = &Alphacolors[n];

	if ( changed && (ac->r!=r || ac->g!=g || ac->b!=b || ac->alpha!=alpha || ac->type != type ) )	{
		// we're changing the color, so delete the old cache file
		//mprintf(( "Changing ac from %d,%d,%d,%d to %d,%d,%d,%d\n", ac->r, ac->g, ac->b, ac->alpha, r, g, b, alpha ));
		//ac_delete_cached(ac);
	}

	ac->used = 1;
	ac->r = r;
	ac->g = g;
	ac->b = b;
	ac->alpha = alpha;
	ac->type = type;
	ac->clr=clr;
	
	calc_alphacolor(ac);

	grx_init_color( clr, r, g, b );

	// Link the alphacolor to the color
	clr->alpha = (unsigned char)alpha;
	clr->ac_type = (ubyte)type;
	clr->alphacolor = n;
	clr->is_alphacolor = 1;
}


void grx_get_color( int * r, int * g, int * b )
{
	if (r) *r = gr_screen.current_color.red;
	if (g) *g = gr_screen.current_color.green;
	if (b) *b = gr_screen.current_color.blue;
}

void grx_init_color( color * dst, int r, int g, int b )
{
	dst->screen_sig = gr_screen.signature;
	dst->red = (unsigned char)r;
	dst->green = (unsigned char)g;
	dst->blue = (unsigned char)b;
	dst->alpha = 255;
	dst->ac_type = AC_TYPE_NONE;
	dst->is_alphacolor = 0;
	dst->alphacolor = -1;
	dst->magic = 0xAC01;

	dst->raw8 = (unsigned char)palette_find( r, g, b );
}

void grx_set_color_fast( color * dst )
{
	if ( dst->magic != 0xAC01 ) return;

	if ( dst->screen_sig != gr_screen.signature )	{
		if ( dst->is_alphacolor )	{
			grx_init_alphacolor( dst, dst->red, dst->green, dst->blue, dst->alpha, dst->ac_type );
		} else {
			grx_init_color( dst, dst->red, dst->green, dst->blue );
		}
	}

	gr_screen.current_color = *dst;
	
	if ( dst->is_alphacolor )	{
		Assert( dst->alphacolor > -1 );
		Assert( dst->alphacolor <= MAX_ALPHACOLORS );
		Assert( Alphacolors[dst->alphacolor].used );

		// Current_alphacolor = &Alphacolors[dst->alphacolor];
		Current_alphacolor = NULL;
	} else {
		Current_alphacolor = NULL;
	}
}


void grx_set_color( int r, int g, int b )
{
	Assert((r >= 0) && (r < 256));
	Assert((g >= 0) && (g < 256));
	Assert((b >= 0) && (b < 256));

//	if ( r!=0 || g!=0 || b!=0 )	{
//		mprintf(( "Setcolor: %d,%d,%d\n", r,g,b ));
//	}
	grx_init_color( &gr_screen.current_color, r, g, b );
	Current_alphacolor = NULL;
}

void calc_alphacolor_hud_type_old( alphacolor_old * ac )
{
	int i,j;
	int tr,tg,tb, Sr, Sg, Sb;
	ubyte * pal;
	int r, g, b, alpha;
	float falpha;

	// Assert(Alphacolors_intited);

//	mprintf(( "Calculating alphacolor for %d,%d,%d,%d\n", ac->r, ac->g, ac->b, ac->alpha ));

	falpha = i2fl(ac->alpha)/255.0f;
	if ( falpha<0.0f ) falpha = 0.0f; else if ( falpha > 1.0f ) falpha = 1.0f;

	alpha = ac->alpha >> 4;
	if (alpha < 0 ) alpha = 0; else if (alpha > 15 ) alpha = 15;
	r = ac->r;
	if (r < 0 ) r = 0; else if (r > 255 ) r = 255;
	g = ac->g;
	if (g < 0 ) g = 0; else if (g > 255 ) g = 255;
	b = ac->b;
	if (b < 0 ) b = 0; else if (b > 255 ) b = 255;

	int ii[16];

	for (j=1; j<15; j++ )	{

		// JAS: Use 1.5/Gamma instead of 1/Gamma because on Adam's
		// PC a gamma of 1.2 makes text look good, but his gamma is
		// really 1.8.   1.8/1.2 = 1.5
		float factor = falpha * (float)pow(i2fl(j)/14.0f, 1.5f/Gr_gamma);
		//float factor = i2fl(j)/14.0f;

		tr = fl2i( i2fl(r) * factor );
		tg = fl2i( i2fl(g) * factor );
		tb = fl2i( i2fl(b) * factor );

		ii[j] = tr;
		if ( tg > ii[j] )	ii[j] = tg;
		if ( tb > ii[j] )	ii[j] = tb;
	}

	pal = gr_palette;

	int m = r;
	if ( g > m ) m = g;
	if ( b > m ) m = b;

	ubyte ri[256], gi[256], bi[256];

	if ( m > 0 )	{
		for (i=0; i<256; i++ )	{
			ri[i] = ubyte((i*r)/m);
			gi[i] = ubyte((i*g)/m);
			bi[i] = ubyte((i*b)/m);
		}
	} else {
		for (i=0; i<256; i++ )	{
			ri[i] = 0;
			gi[i] = 0;
			bi[i] = 0;
		}
	}

	for (i=0; i<256; i++ )	{
		Sr = pal[0];
		Sg = pal[1];
		Sb = pal[2];
		pal += 3;

		int dst_intensity = Sr;
		if ( Sg > dst_intensity ) dst_intensity = Sg;
		if ( Sb > dst_intensity ) dst_intensity = Sb;

		ac->table.lookup[0][i] = (unsigned char)i;

		for (j=1; j<15; j++ )	{

			int tmp_i = max( ii[j], dst_intensity );

			ac->table.lookup[j][i] = (unsigned char)palette_find(ri[tmp_i],gi[tmp_i],bi[tmp_i]);
		}

		float di = (i2fl(Sr)*.30f+i2fl(Sg)*0.60f+i2fl(Sb)*.10f)/255.0f;
		float factor = 0.0f + di*0.75f;

		tr = fl2i( factor*i2fl(r)*falpha );
		tg = fl2i( factor*i2fl(g)*falpha );
		tb = fl2i( factor*i2fl(b)*falpha );

		if ( tr > 255 ) tr = 255; else if ( tr < 0 ) tr = 0;
		if ( tg > 255 ) tg = 255; else if ( tg < 0 ) tg = 0; 
		if ( tb > 255 ) tb = 255; else if ( tb < 0 ) tb = 0;

		ac->table.lookup[15][i] = (unsigned char)palette_find(tr,tg,tb);
		//ac->table.lookup[15][i] = (unsigned char)palette_find(255,0,0);
	}
}

void calc_alphacolor_old(alphacolor_old *ac)
{
	Assert(Fred_running);
	calc_alphacolor_hud_type_old(ac);
}
