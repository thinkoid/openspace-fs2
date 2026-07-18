/*
 * Copyright (C) Volition, Inc. 1999.  All rights reserved.
 *
 * All source code herein is the property of Volition, Inc. You may not sell 
 * or otherwise commercially exploit the source or things you created based on the 
 * source.
 *
*/ 

#ifndef _BMPMAN_H
#define _BMPMAN_H

#include "pstypes.h"

#ifdef FS2_DEMO
	#define MAX_BITMAPS 3500
#else
	#define MAX_BITMAPS 3500			// How many bitmaps the game can handle
#endif

// 16 bit pixel formats
#define BM_PIXEL_FORMAT_ARGB				0						// 1555 LFB writes

// 16 bit pixel formats
extern int Bm_pixel_format;

#define BYTES_PER_PIXEL(x)	((x+7)/8)

// how many bytes of textures are used.
extern int bm_texture_ram;

// This loads a bitmap so we can draw with it later.
// It returns a negative number if it couldn't load
// the bitmap.   On success, it returns the bitmap
// number.
int bm_load(char * filename);

// special load function. basically allows you to load a bitmap which already exists (by filename). 
// this is useful because in some cases we need to have a bitmap which is locked in screen format
// _and_ texture format, such as pilot pics and squad logos
int bm_load_duplicate(char *filename);

// Creates a bitmap that exists in RAM somewhere, instead
// of coming from a disk file.  You pass in a pointer to a
// block of data.  The data can be in the following formats:
// 8 bpp (mapped into game palette)
// 32 bpp
// On success, it returns the bitmap number.  You cannot 
// free that RAM until bm_release is called on that bitmap.  
// See example at bottom of this file
int bm_create( int bpp, int w, int h, void * data, int flags = 0);

// Frees up a bitmap's data, but bitmap number 'n' can
// still be used, it will just have to be paged in next
// time it is locked.
int bm_unload( int n );

// Frees up a bitmap's data, and it's slot, so bitmap 
// number 'n' cannot be used anymore, and bm_load or
// bm_create might reuse the slot.
void bm_release(int n);

// This loads a bitmap sequence so we can draw with it later.
// It returns a negative number if it couldn't load
// the bitmap.   On success, it returns the bitmap
// number of the first frame and nframes is set.
extern int bm_load_animation( char * filename, int * nframes, int *fps = NULL, int can_drop_frames = 0 );

// This locks down a bitmap and returns a pointer to a bitmap
// that can be accessed until you call bm_unlock.   Only lock
// a bitmap when you need it!  This will convert it into the 
// appropriate format also.
extern bitmap * bm_lock( int bitmapnum, ubyte bpp, ubyte flags );

// The signature is a field that gets filled in with 
// a unique signature for each bitmap.  The signature for each bitmap
// will also change when the bitmap's data changes.
extern uint bm_get_signature( int bitmapnum);

// Unlocks a bitmap
extern void bm_unlock( int bitmapnum );

// Gets info.   w,h,or flags,nframes or fps can be NULL if you don't care.
extern void bm_get_info( int bitmapnum, int *w=NULL, int * h=NULL, ubyte * flags=NULL, int *nframes=NULL, int *fps=NULL, bitmap_section_info **sections = NULL );

// get filename
extern void bm_get_filename(int bitmapnum, char *filename);

// resyncs all the bitmap palette
extern void bm_update();

// call to load all data for all bitmaps that have been requested to be loaded
extern void bm_load_all();
extern void bm_unload_all();

// call to get the palette for a bitmap
extern void bm_get_palette(int n, ubyte *pal, char *name);

// Hacked function to get a pixel from a bitmap.
// Only works good in 8bpp mode.
void bm_get_pixel( int bitmap, float u, float v, ubyte *r, ubyte *g, ubyte *b );

// Returns number of bytes of bitmaps locked this frame
// ntotal = number of bytes of bitmaps locked this frame
// nnew = number of bytes of bitmaps locked this frame that weren't locked last frame
void bm_get_frame_usage(int *ntotal, int *nnew);

/* 
 * Example on using bm_create
 * 
	{
		static int test_inited = 0;
		static int test_bmp;
		static uint test_bmp_data[128*64];

		if ( !test_inited )	{
			test_inited = 1;
			// Create the new bitmap and fill in its data.
			// When you're done with it completely, call
			// bm_release to free up the bitmap handle
			test_bmp = bm_create( 32, 128, 64, test_bmp_data );
			int i,j;
			for (i=0; i<64; i++ )	{
				for (j=0; j<64; j++ )	{
					uint r=i*4;
					test_bmp_data[j+i*128] = r;
				}
			}
		}

		bm_unload(test_bmp);	// this pages out the data, so that the
									// next bm_lock will convert the new data to the
									// correct bpp

		// put in new data
		int x,y;
		gr_reset_clip();
		for (y=0; y<64; y++)
			for (x=0; x<128; x++ )
				test_bmp_data[y*128+x] = 15;

		// Draw the bitmap to upper left corner
		gr_set_bitmap(test_bmp);
		gr_bitmap( 0,0 );
	}
*/


//============================================================================
// Paging stuff
//============================================================================

void bm_page_in_start();
void bm_page_in_stop();

// Paging code in a library should call these functions
// in its page in function.

// Marks a texture as being used for this level
// If num_frames is passed, assume this is an animation
void bm_page_in_texture( int bitmapnum, int num_frames=1 );

// Marks a texture as being used for this level
// If num_frames is passed, assume this is an animation
void bm_page_in_nondarkening_texture( int bitmap, int num_frames=1 );

// marks a texture as being a transparent textyre used for this level
// Marks a texture as being used for this level
// If num_frames is passed, assume this is an animation
void bm_page_in_xparent_texture( int bitmapnum, int num_frames=1 );

// Marks an aabitmap as being used for this level
// If num_frames is passed, assume this is an animation
void bm_page_in_aabitmap( int bitmapnum, int num_frames=1 );

// 
// Mode: 0 = High memory
//       1 = Low memory ( every other frame of ani's)
//       2 = Debug low memory ( only use first frame of each ani )
void bm_set_low_mem( int mode );

char *bm_get_filename(int handle);

void BM_SELECT_SCREEN_FORMAT();
void BM_SELECT_TEX_FORMAT();
void BM_SELECT_ALPHA_TEX_FORMAT();

// convert a 24 bit value to a 16 bit value
void bm_24_to_16(int bit_24, ushort *bit_16);

// set the rgba components of a pixel, any of the parameters can be NULL
extern void (*bm_set_components)(ubyte *pixel, ubyte *r, ubyte *g, ubyte *b, ubyte *a);
void bm_set_components_argb(ubyte *pixel, ubyte *r, ubyte *g, ubyte *b, ubyte *a);

// get the rgba components of a pixel, any of the parameters can be NULL
void bm_get_components(ubyte *pixel, ubyte *r, ubyte *g, ubyte *b, ubyte *a);

//============================================================================
// section info stuff
//============================================================================

// given a bitmap and a section, return the size (w, h)
void bm_get_section_size(int bitmapnum, int sx, int sy, int *w, int *h);

#endif
