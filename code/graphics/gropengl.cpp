/*
 * Copyright (C) Volition, Inc. 1999.  All rights reserved.
 *
 * All source code herein is the property of Volition, Inc. You may not sell
 * or otherwise commercially exploit the source or things you created based on the
 * source.
 *
*/

// Retail shipped this file as an unwired skeleton: gr_opengl_* stubs and the
// vtable binding block, no GL calls at all (the win32 GL work never landed in
// the retail drop).  Revived for the port: SDL2 provides the context, the
// stubs get filled in with real GL incrementally.  Reference implementations:
// retail grd3d*.cpp (same vtable contract) and the 2002 fs2open gropengl.cpp.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>

#include "osapi.h"
#include "2d.h"
#include "bmpman.h"
#include "floating.h"
#include "palman.h"
#include "grinternal.h"
#include "gropengl.h"
#include "font.h"
#include "line.h"

static int Inited = 0;

static SDL_GLContext GL_context = NULL;

static int GL_clear_color_r = 0;
static int GL_clear_color_g = 0;
static int GL_clear_color_b = 0;

static int GL_zbuffer_mode = GR_ZBUFF_NONE;

extern uint Gr_signature;

void gr_opengl_pixel(int x, int y)
{
	if ( x < gr_screen.clip_left ) return;
	if ( x > gr_screen.clip_right ) return;
	if ( y < gr_screen.clip_top ) return;
	if ( y > gr_screen.clip_bottom ) return;
}

void gr_opengl_clear()
{
	glClearColor( GL_clear_color_r / 255.0f, GL_clear_color_g / 255.0f, GL_clear_color_b / 255.0f, 1.0f );
	glClear( GL_COLOR_BUFFER_BIT );
}


void gr_opengl_flip()
{
	gr_reset_clip();

	// TEMPORARY bring-up aid: FS2_FRAME_DUMP=<dir> writes every 60th frame
	// as P6 PPM, read back from the back buffer before the swap; same hook
	// as the software renderer, delete when stable
	static int frame_no = 0;
	const char *dumpdir = getenv("FS2_FRAME_DUMP");
	if ( dumpdir && (frame_no++ % 60) == 0 )	{
		int w = gr_screen.max_w, h = gr_screen.max_h;
		ubyte *pixels = (ubyte *)malloc( w * h * 3 );
		if ( pixels )	{
			glPixelStorei( GL_PACK_ALIGNMENT, 1 );
			glReadBuffer( GL_BACK );
			glReadPixels( 0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, pixels );

			char path[512];
			snprintf(path, sizeof(path), "%s/frame%05d.ppm", dumpdir, frame_no);
			FILE *out = fopen(path, "wb");
			if (out)	{
				fprintf(out, "P6\n%d %d\n255\n", w, h);
				for (int y = h-1; y >= 0; y-- )	{	// GL rows are bottom-up
					fwrite( pixels + y * w * 3, 1, w * 3, out );
				}
				fclose(out);
			}
			free(pixels);
		}
	}

	SDL_Window *win = os_get_sdl_window();
	if ( win )	{
		SDL_GL_SwapWindow( win );
	}

	// start the new back buffer deterministic: the two buffers otherwise
	// alternate stale contents (visible as flicker) until the game draws
	// over every pixel
	gr_opengl_clear();
}

void gr_opengl_flip_window(uint _hdc, int x, int y, int w, int h )
{
}

void gr_opengl_set_clip(int x,int y,int w,int h)
{
	// check for sanity of parameters
	if (x < 0)
		x = 0;
	if (y < 0)
		y = 0;

	if (x >= gr_screen.max_w)
		x = gr_screen.max_w - 1;
	if (y >= gr_screen.max_h)
		y = gr_screen.max_h - 1;

	if (x + w > gr_screen.max_w)
		w = gr_screen.max_w - x;
	if (y + h > gr_screen.max_h)
		h = gr_screen.max_h - y;

	if (w > gr_screen.max_w)
		w = gr_screen.max_w;
	if (h > gr_screen.max_h)
		h = gr_screen.max_h;

	gr_screen.offset_x = x;
	gr_screen.offset_y = y;
	gr_screen.clip_left = 0;
	gr_screen.clip_right = w-1;
	gr_screen.clip_top = 0;
	gr_screen.clip_bottom = h-1;
	gr_screen.clip_width = w;
	gr_screen.clip_height = h;
}

void gr_opengl_reset_clip()
{
	gr_screen.offset_x = 0;
	gr_screen.offset_y = 0;
	gr_screen.clip_left = 0;
	gr_screen.clip_top = 0;
	gr_screen.clip_right = gr_screen.max_w - 1;
	gr_screen.clip_bottom = gr_screen.max_h - 1;
	gr_screen.clip_width = gr_screen.max_w;
	gr_screen.clip_height = gr_screen.max_h;
}

void gr_opengl_set_color( int r, int g, int b )
{
	Assert((r >= 0) && (r < 256));
	Assert((g >= 0) && (g < 256));
	Assert((b >= 0) && (b < 256));

	gr_screen.current_color.red = (unsigned char)r;
	gr_screen.current_color.green = (unsigned char)g;
	gr_screen.current_color.blue = (unsigned char)b;
}

void gr_opengl_set_bitmap( int bitmap_num, int alphablend_mode, int bitblt_mode, float alpha, int sx, int sy )
{
	gr_screen.current_alpha = alpha;
	gr_screen.current_alphablend_mode = alphablend_mode;
	gr_screen.current_bitblt_mode = bitblt_mode;
	gr_screen.current_bitmap = bitmap_num;

	gr_screen.current_bitmap_sx = sx;
	gr_screen.current_bitmap_sy = sy;
}

void gr_opengl_create_shader(shader * shade, float r, float g, float b, float c )
{
	shade->screen_sig = gr_screen.signature;
	shade->r = r;
	shade->g = g;
	shade->b = b;
	shade->c = c;
}

void gr_opengl_set_shader( shader * shade )
{
	if ( shade )	{
		if (shade->screen_sig != gr_screen.signature)	{
			gr_create_shader( shade, shade->r, shade->g, shade->b, shade->c );
		}
		gr_screen.current_shader = *shade;
	} else {
		gr_create_shader( &gr_screen.current_shader, 0.0f, 0.0f, 0.0f, 0.0f );
	}
}


void gr_opengl_bitmap_ex(int x,int y,int w,int h,int sx,int sy)
{
	int i,j;
	bitmap * bmp;
	ubyte * sptr;

	bmp = bm_lock( gr_screen.current_bitmap, 8, 0 );
	sptr = (ubyte *)( bmp->data + (sy*bmp->w+sx) );

//	mprintf(( "x=%d, y=%d, w=%d, h=%d\n", x, y, w, h ));
//	mprintf(( "sx=%d, sy=%d, bw=%d, bh=%d\n", sx, sy, bmp->w, bmp->h ));

	for (i=0; i<h; i++ )	{
		for ( j=0; j<w; j++ )	{
			gr_set_color( gr_palette[sptr[j]*3+0], gr_palette[sptr[j]*3+1], gr_palette[sptr[j]*3+2] );
			gr_pixel( x+j, i+y );
		}
		sptr += bmp->w;
	}
	bm_unlock(gr_screen.current_bitmap);
}

void gr_opengl_bitmap(int x, int y)
{
	int w, h;

	bm_get_info( gr_screen.current_bitmap, &w, &h, NULL );
	int dx1=x, dx2=x+w-1;
	int dy1=y, dy2=y+h-1;
	int sx=0, sy=0;

	if ((dx1 > gr_screen.clip_right ) || (dx2 < gr_screen.clip_left)) return;
	if ((dy1 > gr_screen.clip_bottom ) || (dy2 < gr_screen.clip_top)) return;
	if ( dx1 < gr_screen.clip_left ) { sx = gr_screen.clip_left-dx1; dx1 = gr_screen.clip_left; }
	if ( dy1 < gr_screen.clip_top ) { sy = gr_screen.clip_top-dy1; dy1 = gr_screen.clip_top; }
	if ( dx2 > gr_screen.clip_right )	{ dx2 = gr_screen.clip_right; }
	if ( dy2 > gr_screen.clip_bottom )	{ dy2 = gr_screen.clip_bottom; }

	if ( sx < 0 ) return;
	if ( sy < 0 ) return;
	if ( sx >= w ) return;
	if ( sy >= h ) return;

	// Draw bitmap bm[sx,sy] into (dx1,dy1)-(dx2,dy2)

	gr_bitmap_ex(dx1,dy1,dx2-dx1+1,dy2-dy1+1,sx,sy);
}

void gr_opengl_aabitmap_ex(int x, int y, int w, int h, int sx, int sy)
{
}

void gr_opengl_aabitmap(int x, int y)
{
}

static void opengl_scanline(int x1,int x2,int y)
{
}

void gr_opengl_rect(int x,int y,int w,int h)
{
	int i, swapped=0;
	int x1 = x, x2;
	int y1 = y, y2;

	if ( w > 0 )
		 x2 = x + w - 1;
	else
		 x2 = x + w + 1;

	if ( h > 0 )
		y2 = y + h - 1;
	else
		y2 = y + h + 1;

	if ( x2 < x1 )	{
		int tmp;
		tmp = x1;
		x1 = x2;
		x2 = tmp;
		w = -w;
		swapped = 1;
	}

	if ( y2 < y1 )	{
		int tmp;
		tmp = y1;
		y1 = y2;
		y2 = tmp;
		h = -h;
		swapped = 1;
	}

	for (i=0; i<h; i++ )
		opengl_scanline( x1, x2, y1+i );
}


void gr_opengl_shade(int x,int y,int w,int h)
{
}

void opengl_mtext(int x, int y, char *s, int len )
{
}

void gr_opengl_string(int x,int y,char * text)
{
	char *p, *p1;
	int w, h;

	p1 = text;
	do {
		p = strchr( p1, '\n' );
		if ( p ) {
			*p = 0;
			p++;
		}
		gr_get_string_size( &w, &h, p1 );

		if ( x == 0x8000 )
			opengl_mtext(gr_screen.offset_x+(gr_screen.clip_width-w)/2,y+gr_screen.offset_y,p1,strlen(p1));
		else
			opengl_mtext(gr_screen.offset_x+x,y+gr_screen.offset_y,p1,strlen(p1));

		p1 = p;
		if ( p1 && (strlen(p1) < 1) ) p1 = NULL;
		y += h;
	} while(p1!=NULL);
}




void gr_opengl_circle( int xc, int yc, int d )
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
		opengl_scanline( xc-y, xc+y, yc-x );
		opengl_scanline( xc-y, xc+y, yc+x );

		if (p<0)
			p=p+(x<<2)+6;
		else	{
			// Draw the second octant
			opengl_scanline( xc-x, xc+x, yc-y );
			opengl_scanline( xc-x, xc+x, yc+y );
			p=p+((x-y)<<2)+10;
			y--;
		}
		x++;
	}
	if(x==y)	{
		opengl_scanline( xc-x, xc+x, yc-y );
		opengl_scanline( xc-x, xc+x, yc+y );
	}
	return;
}


void gr_opengl_line(int x1,int y1,int x2,int y2)
{
	int i;
   int xstep,ystep;
   int dy=y2-y1;
   int dx=x2-x1;
   int error_term=0;
	int clipped = 0, swapped=0;

	INT_CLIPLINE(x1,y1,x2,y2,gr_screen.clip_left,gr_screen.clip_top,gr_screen.clip_right,gr_screen.clip_bottom,return,clipped=1,swapped=1);

	if(dy<0)	{
		dy=-dy;
      ystep=-1;
	}	else	{
      ystep=1;
	}

   if(dx<0)	{
      dx=-dx;
      xstep=-1;
   } else {
      xstep=1;
	}

	if(dx>dy)	{

		for(i=dx+1;i>0;i--) {
			gr_pixel( x1, y1 );
			x1 += xstep;
         error_term+=dy;

         if(error_term>dx)	{
				error_term-=dx;
            y1+=ystep;
         }
      }
   } else {

      for(i=dy+1;i>0;i--)	{
			gr_pixel( x1, y1 );
			y1 += ystep;
         error_term+=dx;
         if(error_term>0)	{
            error_term-=dy;
            x1+=xstep;
         }

      }

   }
}

void gr_opengl_aaline(vertex *v1, vertex *v2)
{
	gr_opengl_line( fl2i(v1->sx), fl2i(v1->sy), fl2i(v2->sx), fl2i(v2->sy) );
}

#define FIND_SCALED_NUM(x,x0,x1,y0,y1) (((((x)-(x0))*((y1)-(y0)))/((x1)-(x0)))+(y0))

void gr_opengl_scaler(vertex *va, vertex *vb )
{
	float x0, y0, x1, y1;
	float u0, v0, u1, v1;
	float clipped_x0, clipped_y0, clipped_x1, clipped_y1;
	float clipped_u0, clipped_v0, clipped_u1, clipped_v1;
	float xmin, xmax, ymin, ymax;
	int dx0, dy0, dx1, dy1;

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
	ubyte * sbits;
	bitmap * bp;
	ubyte * spixels;
	float tmpu, tmpv;

	tmpu = (clipped_u1-clipped_u0) / (dx1-dx0);
	if ( fl_abs(tmpu) < 0.001f ) {
		return;		// scaled up way too far!
	}
	tmpv = (clipped_v1-clipped_v0) / (dy1-dy0);
	if ( fl_abs(tmpv) < 0.001f ) {
		return;		// scaled up way too far!
	}

	bp = bm_lock( gr_screen.current_bitmap, 8, 0 );

	du = fl2f(tmpu*(bp->w-1));
	dv = fl2f(tmpv*(bp->h-1));

	v = fl2f(clipped_v0*(bp->h-1));
	u = fl2f(clipped_u0*(bp->w-1));
	w = dx1 - dx0 + 1;

	spixels = (ubyte *)bp->data;

	for (y=dy0; y<=dy1; y++ )			{
		sbits = &spixels[bp->rowsize*(v>>16)];

		int x, tmp_u;
		tmp_u = u;
		for (x=0; x<w; x++ )			{
			ubyte c = sbits[ tmp_u >> 16 ];
			if ( c != 255 ) {
				gr_set_color( gr_palette[c*3+0], gr_palette[c*3+1], gr_palette[c*3+2] );
				gr_pixel( x+dx0, y );
			}
			tmp_u += du;
		}
		v += dv;
	}

	bm_unlock(gr_screen.current_bitmap);

}

void gr_opengl_aascaler(vertex *va, vertex *vb )
{
	gr_opengl_scaler( va, vb );
}


void gr_opengl_tmapper( int nv, vertex * verts[], uint flags )
{
}


void gr_opengl_gradient(int x1,int y1,int x2,int y2)
{
}

void gr_opengl_set_palette(ubyte *new_palette, int is_alphacolor)
{
}

void gr_opengl_get_color( int * r, int * g, int * b )
{
	if (r) *r = gr_screen.current_color.red;
	if (g) *g = gr_screen.current_color.green;
	if (b) *b = gr_screen.current_color.blue;
}

void gr_opengl_init_color(color *c, int r, int g, int b)
{
	c->screen_sig = gr_screen.signature;
	c->red = (unsigned char)r;
	c->green = (unsigned char)g;
	c->blue = (unsigned char)b;
}

// hardware-mode alphacolors carry real rgba on the color struct (the software
// renderer builds palette remap tables instead); same shape as gr_d3d_init_alphacolor
void gr_opengl_init_alphacolor( color *clr, int r, int g, int b, int alpha, int type )
{
	if ( r < 0 ) r = 0; else if ( r > 255 ) r = 255;
	if ( g < 0 ) g = 0; else if ( g > 255 ) g = 255;
	if ( b < 0 ) b = 0; else if ( b > 255 ) b = 255;
	if ( alpha < 0 ) alpha = 0; else if ( alpha > 255 ) alpha = 255;

	gr_opengl_init_color( clr, r, g, b );

	clr->alpha = (unsigned char)alpha;
	clr->ac_type = (ubyte)type;
	clr->alphacolor = -1;
	clr->is_alphacolor = 1;
}

void gr_opengl_set_color_fast(color *dst)
{
	if ( dst->screen_sig != gr_screen.signature )	{
		gr_init_color( dst, dst->red, dst->green, dst->blue );
		return;
	}
	gr_screen.current_color = *dst;
}



void gr_opengl_print_screen(char *filename)
{

}

int gr_opengl_supports_res_ingame(int res)
{
	return 1;
}

int gr_opengl_supports_res_interface(int res)
{
	return 1;
}

void gr_opengl_start_frame()
{
}

void gr_opengl_stop_frame()
{
}

void gr_opengl_fade_in(int instantaneous)
{
}

void gr_opengl_fade_out(int instantaneous)
{
}

void gr_opengl_flash( int r, int g, int b )
{
}

int gr_opengl_zbuffer_get()
{
	return GL_zbuffer_mode;
}

int gr_opengl_zbuffer_set(int mode)
{
	int tmp = GL_zbuffer_mode;
	GL_zbuffer_mode = mode;
	return tmp;
}

void gr_opengl_zbuffer_clear(int use_zbuffer)
{
	if ( use_zbuffer )	{
		glClear( GL_DEPTH_BUFFER_BIT );
	}
}

int gr_opengl_save_screen()
{
	// no readback path yet; popups cope with a failed save
	return -1;
}

void gr_opengl_restore_screen(int id)
{
}

void gr_opengl_free_screen(int id)
{
}

void gr_opengl_dump_frame_start( int first_frame_number, int nframes_between_dumps )
{
}

void gr_opengl_dump_frame()
{
}

void gr_opengl_dump_frame_stop()
{
}

void gr_opengl_set_gamma(float gamma)
{
	Gr_gamma = gamma;
	Gr_gamma_int = int(Gr_gamma*100);

	// gamma lookup table, applied when converting textures for upload
	int i;
	for (i=0; i<256; i++ )	{
		int v = fl2i(pow(i2fl(i)/255.0f, 1.0f/Gr_gamma)*255.0f);
		if ( v > 255 ) {
			v = 255;
		} else if ( v < 0 )	{
			v = 0;
		}
		Gr_gamma_lookup[i] = v;
	}

	gr_screen.signature = Gr_signature++;
}

uint gr_opengl_lock()
{
	return 1;
}

void gr_opengl_unlock()
{
}

void gr_opengl_cleanup()
{
	if ( !Inited )	return;

	gr_reset_clip();
	gr_clear();
	gr_flip();

	if ( GL_context )	{
		SDL_GL_DeleteContext( GL_context );
		GL_context = NULL;
	}

	Inited = 0;
}

void gr_opengl_fog_set(int fog_mode, int r, int g, int b, float near, float far)
{
}

void gr_opengl_get_pixel(int x, int y, int *r, int *g, int *b)
{
}

void gr_opengl_get_region(int front, int w, int g, ubyte *data)
{
}

void gr_opengl_set_cull(int cull)
{
}

void gr_opengl_filter_set(int filter)
{
}

// cross fade
void gr_opengl_cross_fade(int bmap1, int bmap2, int x1, int y1, int x2, int y2, float pct)
{
}

int gr_opengl_tcache_set(int bitmap_id, int bitmap_type, float *u_ratio, float *v_ratio, int fail_on_full = 0, int sx = -1, int sy = -1, int force = 0)
{
	return 1;
}

void gr_opengl_set_clear_color(int r, int g, int b)
{
	GL_clear_color_r = r;
	GL_clear_color_g = g;
	GL_clear_color_b = b;
}

void gr_opengl_activate(int active)
{
}

void gr_opengl_force_windowed()
{
}

void gr_opengl_init()
{
	if ( Inited )	{
		gr_opengl_cleanup();
		Inited = 0;
	}

	mprintf(( "Initializing opengl graphics device...\n" ));

	// context attributes must be set before the window is created
	SDL_GL_SetAttribute( SDL_GL_DOUBLEBUFFER, 1 );
	SDL_GL_SetAttribute( SDL_GL_DEPTH_SIZE, 24 );

	if ( os_create_window( gr_screen.max_w, gr_screen.max_h, 1 ) )	{
		Error( LOCATION, "Can't create window for OpenGL" );
	}

	GL_context = SDL_GL_CreateContext( os_get_sdl_window() );
	if ( !GL_context )	{
		Error( LOCATION, "SDL_GL_CreateContext failed: %s", SDL_GetError() );
	}

	SDL_GL_SetSwapInterval( 1 );

	mprintf(( "  Vendor   : %s\n", (const char *)glGetString(GL_VENDOR) ));
	mprintf(( "  Renderer : %s\n", (const char *)glGetString(GL_RENDERER) ));
	mprintf(( "  Version  : %s\n", (const char *)glGetString(GL_VERSION) ));

	glViewport( 0, 0, gr_screen.max_w, gr_screen.max_h );

	// hardware mode is 16bpp to the rest of the game (bm_lock, bm_set_components);
	// 1555 ARGB, the same guns the D3D backend reported
	Gr_red.bits = 5;	Gr_red.shift = 10;	Gr_red.scale = 256/32;	Gr_red.mask = 0x7C00;
	Gr_green.bits = 5;	Gr_green.shift = 5;	Gr_green.scale = 256/32;	Gr_green.mask = 0x03e0;
	Gr_blue.bits = 5;	Gr_blue.shift = 0;	Gr_blue.scale = 256/32;	Gr_blue.mask = 0x1F;
	Gr_alpha.bits = 1;	Gr_alpha.shift = 15;	Gr_alpha.scale = 255;	Gr_alpha.mask = 0x8000;
	Gr_t_red = Gr_red;	Gr_t_green = Gr_green;	Gr_t_blue = Gr_blue;	Gr_t_alpha = Gr_alpha;
	Gr_current_red = &Gr_red;	Gr_current_green = &Gr_green;
	Gr_current_blue = &Gr_blue;	Gr_current_alpha = &Gr_alpha;

	gr_screen.bits_per_pixel = 16;
	gr_screen.bytes_per_pixel = 2;

	Inited = 1;

	gr_screen.gf_flip = gr_opengl_flip;
	gr_screen.gf_flip_window = gr_opengl_flip_window;
	gr_screen.gf_set_clip = gr_opengl_set_clip;
	gr_screen.gf_reset_clip = gr_opengl_reset_clip;
	gr_screen.gf_set_font = grx_set_font;
	gr_screen.gf_set_color = gr_opengl_set_color;
	gr_screen.gf_set_bitmap = gr_opengl_set_bitmap;
	gr_screen.gf_create_shader = gr_opengl_create_shader;
	gr_screen.gf_set_shader = gr_opengl_set_shader;
	gr_screen.gf_clear = gr_opengl_clear;
	// gr_screen.gf_bitmap = gr_opengl_bitmap;
	// gr_screen.gf_bitmap_ex = gr_opengl_bitmap_ex;

	gr_screen.gf_aabitmap = gr_opengl_aabitmap;
	gr_screen.gf_aabitmap_ex = gr_opengl_aabitmap_ex;

	gr_screen.gf_rect = gr_opengl_rect;
	gr_screen.gf_shade = gr_opengl_shade;
	gr_screen.gf_string = gr_opengl_string;
	gr_screen.gf_circle = gr_opengl_circle;

	gr_screen.gf_line = gr_opengl_line;
	gr_screen.gf_aaline = gr_opengl_aaline;
	gr_screen.gf_pixel = gr_opengl_pixel;
	gr_screen.gf_scaler = gr_opengl_scaler;
	gr_screen.gf_aascaler = gr_opengl_aascaler;
	gr_screen.gf_tmapper = gr_opengl_tmapper;

	gr_screen.gf_gradient = gr_opengl_gradient;

	gr_screen.gf_set_palette = gr_opengl_set_palette;
	gr_screen.gf_get_color = gr_opengl_get_color;
	gr_screen.gf_init_color = gr_opengl_init_color;
	gr_screen.gf_init_alphacolor = gr_opengl_init_alphacolor;
	gr_screen.gf_set_color_fast = gr_opengl_set_color_fast;
	gr_screen.gf_print_screen = gr_opengl_print_screen;
	gr_screen.gf_start_frame = gr_opengl_start_frame;
	gr_screen.gf_stop_frame = gr_opengl_stop_frame;

	gr_screen.gf_fade_in = gr_opengl_fade_in;
	gr_screen.gf_fade_out = gr_opengl_fade_out;
	gr_screen.gf_flash = gr_opengl_flash;

	gr_screen.gf_zbuffer_get = gr_opengl_zbuffer_get;
	gr_screen.gf_zbuffer_set = gr_opengl_zbuffer_set;
	gr_screen.gf_zbuffer_clear = gr_opengl_zbuffer_clear;

	gr_screen.gf_save_screen = gr_opengl_save_screen;
	gr_screen.gf_restore_screen = gr_opengl_restore_screen;
	gr_screen.gf_free_screen = gr_opengl_free_screen;

	gr_screen.gf_dump_frame_start = gr_opengl_dump_frame_start;
	gr_screen.gf_dump_frame_stop = gr_opengl_dump_frame_stop;
	gr_screen.gf_dump_frame = gr_opengl_dump_frame;

	gr_screen.gf_set_gamma = gr_opengl_set_gamma;

	gr_screen.gf_lock = gr_opengl_lock;
	gr_screen.gf_unlock = gr_opengl_unlock;

	gr_screen.gf_get_region = gr_opengl_get_region;

	gr_screen.gf_fog_set = gr_opengl_fog_set;

	gr_screen.gf_get_pixel = gr_opengl_get_pixel;

	gr_screen.gf_set_cull = gr_opengl_set_cull;

	gr_screen.gf_cross_fade = gr_opengl_cross_fade;

	gr_screen.gf_filter_set = gr_opengl_filter_set;

	gr_screen.gf_tcache_set = gr_opengl_tcache_set;

	gr_screen.gf_set_clear_color = gr_opengl_set_clear_color;

	gr_reset_clip();
	gr_clear();
	gr_flip();
}
