/*
 * Copyright (C) Volition, Inc. 1999.  All rights reserved.
 *
 * All source code herein is the property of Volition, Inc. You may not sell 
 * or otherwise commercially exploit the source or things you created based on the 
 * source.
 *
*/ 

#include <math.h>
#include <windows.h>
#include <windowsx.h>

#include "osapi.h"
#include "2d.h"
#include "bmpman.h"
#include "key.h"
#include "floating.h"
#include "palman.h"
#include "grsoft.h"
#include "grinternal.h"

// Headers for 2d functions
#include "pixel.h"
#include "line.h"
#include "scaler.h"
#include "tmapper.h"
#include "circle.h"
#include "shade.h"
#include "rect.h"
#include "gradient.h"
#include "pcxutils.h"
#include "osapi.h"
#include "mouse.h"
#include "font.h"
#include "timer.h"
#include "colors.h"
#include "bitblt.h"

 // Window's specific

// This structure is the same as LOGPALETTE except that LOGPALETTE
// requires you to malloc out space for the palette, which just isn't
// worth the trouble.

typedef struct {
    WORD         palVersion; 
    WORD         palNumEntries; 
    PALETTEENTRY palPalEntry[256]; 
} EZ_LOGPALETTE; 

// This structure is the same as BITMAPINFO except that BITMAPINFO
// requires you to malloc out space for the palette, which just isn't
// worth the trouble.   I also went ahead and threw a handy union to
// easily reference the hicolor masks in 15/16 bpp modes.
typedef struct	{
	BITMAPINFOHEADER Header;
	union {
		RGBQUAD aColors[256];
		ushort PalIndex[256];
		uint hicolor_masks[3];
	} Colors;
} EZ_BITMAPINFO;

EZ_BITMAPINFO DibInfo;
HBITMAP hDibSection = NULL;
HBITMAP hOldBitmap = NULL;
HDC hDibDC = NULL;
void *lpDibBits=NULL;

HPALETTE hOldPalette=NULL, hPalette = NULL;	

int Gr_soft_inited = 0;

static volatile int Grsoft_activated = 0;				// If set, that means application got focus, so reset palette

void gr_buffer_release()
{
	if ( hPalette )	{
		if (hDibDC)
			SelectPalette( hDibDC, hOldPalette, FALSE );
		if (!DeleteObject(hPalette))	{
			mprintf(( "JOHN: Couldn't delete palette object\n" ));
		}
		hPalette = NULL;
	}

	if ( hDibDC )	{
		SelectObject(hDibDC, hOldBitmap );
		DeleteDC(hDibDC);
		hDibDC = NULL;
	}

	if ( hDibSection )	{
		DeleteObject(hDibSection);
		hDibSection = NULL;
	}
}


void gr_buffer_create( int w, int h, int bpp )
{
	int i;

	if (w & 3) {
		Int3();	// w must be multiple 4
		return;
	}

	gr_buffer_release();

	memset( &DibInfo, 0, sizeof(EZ_BITMAPINFO));
   DibInfo.Header.biSize = sizeof(BITMAPINFOHEADER);
	DibInfo.Header.biWidth = w;
	DibInfo.Header.biHeight = h;
	DibInfo.Header.biPlanes = 1; 
	DibInfo.Header.biClrUsed = 0;

	switch( bpp )	{
	case 8:
		Gr_red.bits = 8;
		Gr_red.shift = 16;
		Gr_red.scale = 1;
		Gr_red.mask = 0xff0000;

		Gr_green.bits = 8;
		Gr_green.shift = 8;
		Gr_green.scale = 1;
		Gr_green.mask = 0xff00;

		Gr_blue.bits = 8;
		Gr_blue.shift = 0;
		Gr_blue.scale = 1;
		Gr_blue.mask = 0xff;

		DibInfo.Header.biCompression = BI_RGB; 
		DibInfo.Header.biBitCount = 8; 
		for (i=0; i<256; i++ )	{
			DibInfo.Colors.aColors[i].rgbRed = 0;
			DibInfo.Colors.aColors[i].rgbGreen = 0;
			DibInfo.Colors.aColors[i].rgbBlue = 0;
			DibInfo.Colors.aColors[i].rgbReserved = 0;
		}
		break;

	case 15:
		Gr_red.bits = 5;
		Gr_red.shift = 10;
		Gr_red.scale = 8;
		Gr_red.mask = 0x7C00;

		Gr_green.bits = 5;
		Gr_green.shift = 5;
		Gr_green.scale = 8;
		Gr_green.mask = 0x3E0;

		Gr_blue.bits = 5;
		Gr_blue.shift = 0;
		Gr_blue.scale = 8;
		Gr_blue.mask = 0x1F;

		DibInfo.Header.biCompression = BI_BITFIELDS;
		DibInfo.Header.biBitCount = 16; 
		DibInfo.Colors.hicolor_masks[0] = Gr_red.mask;
		DibInfo.Colors.hicolor_masks[1] = Gr_green.mask;
		DibInfo.Colors.hicolor_masks[2] = Gr_blue.mask;


		break;

	case 16:
		Gr_red.bits = 5;
		Gr_red.shift = 11;
		Gr_red.scale = 8;
		Gr_red.mask = 0xF800;

		Gr_green.bits = 6;
		Gr_green.shift = 5;
		Gr_green.scale = 4;
		Gr_green.mask = 0x7E0;

		Gr_blue.bits = 5;
		Gr_blue.shift = 0;
		Gr_blue.scale = 8;
		Gr_blue.mask = 0x1F;

		DibInfo.Header.biCompression = BI_BITFIELDS;
		DibInfo.Header.biBitCount = 16; 
		DibInfo.Colors.hicolor_masks[0] = Gr_red.mask;
		DibInfo.Colors.hicolor_masks[1] = Gr_green.mask;
		DibInfo.Colors.hicolor_masks[2] = Gr_blue.mask;
		break;

	case 24:
	case 32:
		Gr_red.bits = 8;
		Gr_red.shift = 16;
		Gr_red.scale = 1;
		Gr_red.mask = 0xff0000;

		Gr_green.bits = 8;
		Gr_green.shift = 8;
		Gr_green.scale = 1;
		Gr_green.mask = 0xff00;

		Gr_blue.bits = 8;
		Gr_blue.shift = 0;
		Gr_blue.scale = 1;
		Gr_blue.mask = 0xff;

		DibInfo.Header.biCompression = BI_RGB; 
		DibInfo.Header.biBitCount = unsigned short(bpp); 
		break;

	default:
		Int3();	// Illegal bpp
	}

	lpDibBits = NULL;

	hDibDC = CreateCompatibleDC(NULL);
	hDibSection = CreateDIBSection(hDibDC,(BITMAPINFO *)&DibInfo,DIB_RGB_COLORS,&lpDibBits,NULL,NULL);
	hOldBitmap = (HBITMAP)SelectObject(hDibDC, hDibSection );

	if ( hDibSection == NULL )	{
		Int3();	// couldn't allocate dib section
	}
}


// This makes a best-fit palette from the 256 target colors and
// the system colors which should look better than only using the
// colors in the range 10-246, but it will totally reorder the palette.
// All colors get changed in target_palette.


HPALETTE gr_create_palette_0(ubyte * target_palette)
{
	EZ_LOGPALETTE LogicalPalette;
	HDC ScreenDC;
	HPALETTE hpal,hpalOld;
	PALETTEENTRY pe[256];
	int NumSysColors, NumColors;
	int i;

	// Create a 1-1 mapping of the system palette
	LogicalPalette.palVersion = 0x300;
	LogicalPalette.palNumEntries = 256;

	// Pack in all the colors
	for (i=0;i<256; i++)	{
		LogicalPalette.palPalEntry[i].peRed = target_palette[i*3+0];
		LogicalPalette.palPalEntry[i].peGreen = target_palette[i*3+1];
		LogicalPalette.palPalEntry[i].peBlue = target_palette[i*3+2];
		LogicalPalette.palPalEntry[i].peFlags = 0;	//PC_EXPLICIT;
	} 

	hpal = CreatePalette( (LOGPALETTE *)&LogicalPalette );

	ScreenDC = CreateCompatibleDC(NULL);

	if ( !(GetDeviceCaps(ScreenDC,RASTERCAPS) & RC_PALETTE) ) {
		DeleteDC(ScreenDC);
		return hpal;
	}
	 
	NumSysColors = GetDeviceCaps( ScreenDC, NUMCOLORS );
	NumColors = GetDeviceCaps( ScreenDC, SIZEPALETTE );

	// Reset all the Palette Manager tables
	SetSystemPaletteUse( ScreenDC, SYSPAL_NOSTATIC );
	SetSystemPaletteUse( ScreenDC, SYSPAL_STATIC );

	// Enter our palette's values into the free slots
	hpalOld=SelectPalette( ScreenDC, hpal, FALSE );
	RealizePalette( ScreenDC );
	SelectPalette( ScreenDC, hpalOld, FALSE );

	GetSystemPaletteEntries(ScreenDC,0,NumColors,pe);

	for (i=0; i<NumSysColors/2; i++ )	{
		pe[i].peFlags = 0;
	}
	for (; i<NumColors - NumSysColors/2; i++ )	{
		pe[i].peFlags = PC_NOCOLLAPSE;
	}
	for (; i<NumColors; i++ )	{
		pe[i].peFlags = 0;
	}
	ResizePalette( hpal, NumColors);
	SetPaletteEntries( hpal, 0, NumColors, pe );
		
	for (i=0; i<256; i++ )	{
		target_palette[i*3+0] = pe[i].peRed;
		target_palette[i*3+1] = pe[i].peGreen;
		target_palette[i*3+2] = pe[i].peBlue;
	}

	DeleteDC(ScreenDC);

	return hpal;
}

HPALETTE gr_create_palette_256( ubyte * target_palette )
{
	EZ_LOGPALETTE LogicalPalette;
	int i;

	// Pack in all the colors
	for (i=0;i<256; i++)	{
		LogicalPalette.palPalEntry[i].peRed = target_palette[i*3+0];
		LogicalPalette.palPalEntry[i].peGreen = target_palette[i*3+1];
		LogicalPalette.palPalEntry[i].peBlue = target_palette[i*3+2];
		LogicalPalette.palPalEntry[i].peFlags = 0;	//PC_RESERVED;	//PC_EXPLICIT;
	} 

	// Create a 1-1 mapping of the system palette
	LogicalPalette.palVersion = 0x300;
	LogicalPalette.palNumEntries = 256;

	return CreatePalette( (LOGPALETTE *)&LogicalPalette );
}

// This makes an indentity logical palette that saves entries 10-246
// and leaves them in place.  Colors 0-9 and 246-255 get changed in 
// target_palette.  trash_flag tells us whether to trash the system palette
// or not
HPALETTE gr_create_palette_236( ubyte * target_palette )
{
	EZ_LOGPALETTE LogicalPalette;
	HDC ScreenDC;
	int NumSysColors, NumColors, UserLowest, UserHighest;
	int i;

	// Pack in all the colors
	for (i=0;i<256; i++)	{
		LogicalPalette.palPalEntry[i].peRed = target_palette[i*3+0];
		LogicalPalette.palPalEntry[i].peGreen = target_palette[i*3+1];
		LogicalPalette.palPalEntry[i].peBlue = target_palette[i*3+2];
		LogicalPalette.palPalEntry[i].peFlags = 0;	//PC_EXPLICIT;
	} 

	// Create a 1-1 mapping of the system palette
	LogicalPalette.palVersion = 0x300;
	LogicalPalette.palNumEntries = 256;

	ScreenDC = CreateCompatibleDC(NULL);

	// Reset all the Palette Manager tables
	SetSystemPaletteUse( ScreenDC, SYSPAL_NOSTATIC );
	SetSystemPaletteUse( ScreenDC, SYSPAL_STATIC );
		
	if ( !(GetDeviceCaps(ScreenDC,RASTERCAPS) & RC_PALETTE) ) {
		DeleteDC(ScreenDC);
		return CreatePalette( (LOGPALETTE *)&LogicalPalette );
	}

	NumSysColors = GetDeviceCaps( ScreenDC, NUMCOLORS );
	NumColors = GetDeviceCaps( ScreenDC, SIZEPALETTE );

	Assert( NumColors <= 256 );

	UserLowest = NumSysColors/2;								// 10 normally
	UserHighest = NumColors - NumSysColors/2 - 1;		// 245 normally

	Assert( (UserHighest - UserLowest + 1) >= 236 );
			
	GetSystemPaletteEntries(ScreenDC,0,NumSysColors/2,LogicalPalette.palPalEntry);
	GetSystemPaletteEntries(ScreenDC,UserHighest+1,NumSysColors/2,LogicalPalette.palPalEntry+1+UserHighest);

	DeleteDC(ScreenDC);
		
	for (i=0; i<256; i++ )	{

		if ( (i >= UserLowest) && (i<=UserHighest) )	{
			LogicalPalette.palPalEntry[i].peFlags = PC_NOCOLLAPSE;
		} else
			LogicalPalette.palPalEntry[i].peFlags = 0;

		target_palette[i*3+0] = LogicalPalette.palPalEntry[i].peRed;
		target_palette[i*3+1] = LogicalPalette.palPalEntry[i].peGreen;
		target_palette[i*3+2] = LogicalPalette.palPalEntry[i].peBlue;
	}

	return CreatePalette( (LOGPALETTE *)&LogicalPalette );
}

HPALETTE gr_create_palette_254( ubyte * target_palette )
{
	EZ_LOGPALETTE LogicalPalette;
	HDC ScreenDC;
	int NumSysColors, NumColors, UserLowest, UserHighest;
	int i;

	// Pack in all the colors
	for (i=0;i<256; i++)	{
		LogicalPalette.palPalEntry[i].peRed = target_palette[i*3+0];
		LogicalPalette.palPalEntry[i].peGreen = target_palette[i*3+1];
		LogicalPalette.palPalEntry[i].peBlue = target_palette[i*3+2];
		LogicalPalette.palPalEntry[i].peFlags = 0;	//PC_EXPLICIT;
	} 

	// Create a 1-1 mapping of the system palette
	LogicalPalette.palVersion = 0x300;
	LogicalPalette.palNumEntries = 256;

	ScreenDC = CreateCompatibleDC(NULL);

	// Reset all the Palette Manager tables
	SetSystemPaletteUse( ScreenDC, SYSPAL_NOSTATIC );
	SetSystemPaletteUse( ScreenDC, SYSPAL_STATIC );
		
	if ( !(GetDeviceCaps(ScreenDC,RASTERCAPS) & RC_PALETTE) ) {
		DeleteDC(ScreenDC);
		return CreatePalette( (LOGPALETTE *)&LogicalPalette );
	}

	SetSystemPaletteUse( ScreenDC, SYSPAL_NOSTATIC );
	NumSysColors = 2;
	NumColors = GetDeviceCaps( ScreenDC, SIZEPALETTE );

	Assert( NumColors <= 256 );

	UserLowest = NumSysColors/2;								// 10 normally
	UserHighest = NumColors - NumSysColors/2 - 1;		// 245 normally

	Assert( (UserHighest - UserLowest + 1) >= 236 );
			
	GetSystemPaletteEntries(ScreenDC,0,NumSysColors/2,LogicalPalette.palPalEntry);
	GetSystemPaletteEntries(ScreenDC,UserHighest+1,NumSysColors/2,LogicalPalette.palPalEntry+1+UserHighest);

	DeleteDC(ScreenDC);
	
	for (i=0; i<256; i++ )	{

		if ( (i >= UserLowest) && (i<=UserHighest) )	{
			LogicalPalette.palPalEntry[i].peFlags = PC_NOCOLLAPSE;
		} else
			LogicalPalette.palPalEntry[i].peFlags = 0;

		target_palette[i*3+0] = LogicalPalette.palPalEntry[i].peRed;
		target_palette[i*3+1] = LogicalPalette.palPalEntry[i].peGreen;
		target_palette[i*3+2] = LogicalPalette.palPalEntry[i].peBlue;
	}

	return CreatePalette( (LOGPALETTE *)&LogicalPalette );
}

void grx_set_palette_internal( ubyte * new_pal )
{
	if ( hPalette )	{
		if (hDibDC)
			SelectPalette( hDibDC, hOldPalette, FALSE );
		if (!DeleteObject(hPalette))	{
			mprintf(( "JOHN: Couldn't delete palette object\n" ));
		}
		hPalette = NULL;
	}


	// Make sure color 0 is black
	if ( (new_pal[0]!=0) || (new_pal[1]!=0) || (new_pal[2]!=0) )	{
		// color 0 isn't black!! switch it!
		int i;
		int black_index = -1;

		for (i=1; i<256; i++ )	{
			if ( (new_pal[i*3+0]==0) && (new_pal[i*3+1]==0) && (new_pal[i*3+2]==0) )	{	
				black_index = i;
				break;
			}
		}
		if ( black_index > -1 )	{
			// swap black and color 0, so color 0 is black
			ubyte tmp[3];
			tmp[0] = new_pal[black_index*3+0];
			tmp[1] = new_pal[black_index*3+1];
			tmp[2] = new_pal[black_index*3+2];

			new_pal[black_index*3+0] = new_pal[0];
			new_pal[black_index*3+1] = new_pal[1];
			new_pal[black_index*3+2] = new_pal[2];

			new_pal[0] = tmp[0];
			new_pal[1] = tmp[1];
			new_pal[2] = tmp[2];
		} else {
			// no black in palette, force color 0 to be black.
			new_pal[0] = 0;
			new_pal[1] = 0;
			new_pal[2] = 0;
		}
	}



	if ( gr_screen.bits_per_pixel==8 )	{

		// Name                    n_preserved  One-one     Speed      Windowed? 
		// -------------------------------------------------------------------
		// gr_create_palette_256   256          0-255       Slow       Yes
		// gr_create_palette_254   254          1-254       Fast       No
		// gr_create_palette_236   236          10-245      Fast       Yes
		// gr_create_palette_0     0            none        Fast       Yes		

/*
		n_preserved = 256;

		if ( n_preserved <= 0 )	{
			hPalette = gr_create_palette_0(new_pal);	// No colors mapped one-to-one, but probably has close to all 256 colors in it somewhere.
		} else if ( n_preserved <= 236 )	{
			hPalette = gr_create_palette_236(new_pal);	// All colors except low 10 and high 10 mapped one-to-one
		} else if ( n_preserved <= 254 )	{
			hPalette = gr_create_palette_254(new_pal);	// All colors except 0 and 255 mapped one-to-one, but changes system colors.  Not pretty in a window.
		} else {
*/
		hPalette = gr_create_palette_256(new_pal);	// All 256 mapped one-to-one, but BLT's are slow.

		if ( hDibDC )	{
			int i; 
			for (i=0; i<256; i++ )	{
				DibInfo.Colors.aColors[i].rgbRed = new_pal[i*3+0];
				DibInfo.Colors.aColors[i].rgbGreen = new_pal[i*3+1];
				DibInfo.Colors.aColors[i].rgbBlue = new_pal[i*3+2];
				DibInfo.Colors.aColors[i].rgbReserved = 0;
			}

			hOldPalette = SelectPalette( hDibDC, hPalette, FALSE );
			SetDIBColorTable( hDibDC, 0, 256, DibInfo.Colors.aColors );
		}
	} else {
		hPalette = NULL;
	}
}



void grx_set_palette( ubyte * new_pal, int is_alphacolor )
{
	if ( hPalette )	{
		Mouse_hidden++;
		gr_reset_clip();
		gr_clear();
		gr_flip();
		Mouse_hidden--;
	}

	grx_set_palette_internal(new_pal);
}


void grx_print_screen(char * filename)
{
	int i;
	ubyte **row_data = (ubyte **)malloc( gr_screen.max_h * sizeof(ubyte *) );
	if ( !row_data )	{
		mprintf(( "couldn't allocate enough memory to dump screen\n" ));
		return;
	}

	gr_lock();

	for (i=0; i<gr_screen.max_h; i++ )	{
		row_data[i] = GR_SCREEN_PTR(ubyte,0,i);
	}

	pcx_write_bitmap( filename, gr_screen.max_w, gr_screen.max_h, row_data, Gr_current_palette );

	gr_unlock();

	free(row_data);
}


uint gr_soft_lock()
{
	return 1;
}

void gr_soft_unlock()
{
}


void grx_set_palette_internal( ubyte * new_pal );

int Grx_mouse_saved = 0;
int Grx_mouse_saved_x1 = 0;
int Grx_mouse_saved_y1 = 0;
int Grx_mouse_saved_x2 = 0;
int Grx_mouse_saved_y2 = 0;
int Grx_mouse_saved_w = 0;
int Grx_mouse_saved_h = 0;
#define MAX_SAVE_SIZE (32*32)
ubyte Grx_mouse_saved_data[MAX_SAVE_SIZE];

// Clamps X between R1 and R2
#define CLAMP(x,r1,r2) do { if ( (x) < (r1) ) (x) = (r1); else if ((x) > (r2)) (x) = (r2); } while(0)

void grx_save_mouse_area(int x, int y, int w, int h )
{
	Grx_mouse_saved_x1 = x; 
	Grx_mouse_saved_y1 = y;
	Grx_mouse_saved_x2 = x+w-1;
	Grx_mouse_saved_y2 = y+h-1;
	 
	CLAMP(Grx_mouse_saved_x1, gr_screen.clip_left, gr_screen.clip_right );
	CLAMP(Grx_mouse_saved_x2, gr_screen.clip_left, gr_screen.clip_right );
	CLAMP(Grx_mouse_saved_y1, gr_screen.clip_top, gr_screen.clip_bottom );
	CLAMP(Grx_mouse_saved_y2, gr_screen.clip_top, gr_screen.clip_bottom );

	Grx_mouse_saved_w = Grx_mouse_saved_x2 - Grx_mouse_saved_x1 + 1;
	Grx_mouse_saved_h = Grx_mouse_saved_y2 - Grx_mouse_saved_y1 + 1;

	if ( Grx_mouse_saved_w < 1 ) return;
	if ( Grx_mouse_saved_h < 1 ) return;

	// Make sure we're not saving too much!
	Assert( (Grx_mouse_saved_w*Grx_mouse_saved_h) <= MAX_SAVE_SIZE );

	Grx_mouse_saved = 1;

	gr_lock();

	ubyte *sptr, *dptr;

	dptr = Grx_mouse_saved_data;

	for (int i=0; i<Grx_mouse_saved_h; i++ )	{
		sptr = GR_SCREEN_PTR(ubyte,Grx_mouse_saved_x1,Grx_mouse_saved_y1+i);

		for(int j=0; j<Grx_mouse_saved_w; j++ )	{
			*dptr++ = *sptr++;
		}
	}

	gr_unlock();
}


void grx_restore_mouse_area()
{
	if ( !Grx_mouse_saved )	{
		return;
	}

	gr_lock();

	ubyte *sptr, *dptr;

	sptr = Grx_mouse_saved_data;

	for (int i=0; i<Grx_mouse_saved_h; i++ )	{
		dptr = GR_SCREEN_PTR(ubyte,Grx_mouse_saved_x1,Grx_mouse_saved_y1+i);

		for(int j=0; j<Grx_mouse_saved_w; j++ )	{
			*dptr++ = *sptr++;
		}
	}

	gr_unlock();
}


void gr_soft_activate(int active)
{
	if ( active  )	{
		Grsoft_activated++;
	}
}



static int Palette_flashed = 0;
static int Palette_flashed_last_frame = 0;

void grx_change_palette( ubyte *pal );

void grx_flip()
{
	if ( (!Palette_flashed) && (Palette_flashed_last_frame) )	{
		// Reset flash
		grx_change_palette( gr_palette );
	}

	Palette_flashed_last_frame = Palette_flashed;
	Palette_flashed = 0;

	// If program reactivated, flip set new palette.
	// We do the cnt temporary variable because Grsoft_activated
	// can be set during interrupts.

	int cnt = Grsoft_activated;
	if ( cnt )	{
		Grsoft_activated -= cnt;

		ubyte new_pal[768];
		memcpy( new_pal, gr_palette, 768 );
		grx_set_palette_internal( new_pal );		// Call internal one so it doesn't clear screen and call flip
	}

	gr_reset_clip();

//	if (0) {
//		int i;
//		for (i=0; i<gr_screen.max_h; i++ )	{
//			memset( gr_screen.row_data[i], i & 255, abs(gr_screen.rowsize) );
//		}
//	}

	int mx, my;

	Grx_mouse_saved = 0;		// assume not saved

	mouse_eval_deltas();
	if ( mouse_is_visible() )	{
		gr_reset_clip();
		mouse_get_pos( &mx, &my );
		grx_save_mouse_area(mx,my,32,32);
		if ( Gr_cursor == -1 )	{
			gr_set_color(255,255,255);
			gr_line( mx, my, mx+7, my + 7 );
			gr_line( mx, my, mx+5, my );
			gr_line( mx, my, mx, my+5 );
		} else {
			gr_set_bitmap(Gr_cursor);
			gr_bitmap( mx, my );
		}
	} 

	fix t1, t2, d, t;

	HWND hwnd = (HWND)os_get_window();

	if ( hwnd )	{
		int x = gr_screen.offset_x;
		int y = gr_screen.offset_y;
		int w = gr_screen.clip_width;
		int h = gr_screen.clip_height;

		HPALETTE hOldPalette = NULL;
		HDC hdc = GetDC(hwnd);

		if ( hdc )	{
			t1 = timer_get_fixed_seconds();

			if (hPalette)	{
				hOldPalette=SelectPalette(hdc,hPalette, FALSE );
				uint nColors = RealizePalette( hdc );
				nColors;
				//if (nColors)	mprintf(( "Actually set %d palette colors.\n", nColors ));
			}

#if 0 
			BitBlt(hdc,0,h/2,w,h/2,hDibDC,x,y+h/2,SRCCOPY);
#else
			BitBlt(hdc,0,0,w,h,hDibDC,x,y,SRCCOPY);
#endif

			if ( hOldPalette )	
				SelectPalette(hdc,hOldPalette, FALSE );

			ReleaseDC( hwnd, hdc );

			t2 = timer_get_fixed_seconds();
			d = t2 - t1;
			t = (w*h*gr_screen.bytes_per_pixel)/1024;
			//mprintf(( "%d MB/s\n", fixmuldiv(t,65,d) ));

		}
	}

	if ( Grx_mouse_saved )	{
		grx_restore_mouse_area();
	}
}


// switch onscreen, offscreen
// Set msg to 0 if calling outside of the window handler.
void grx_flip_window(uint _hdc, int x, int y, int w, int h )
{
	HDC hdc = (HDC)_hdc;
	HPALETTE hOldPalette = NULL;
	int min_w, min_h;

	if (hPalette)	{
		hOldPalette=SelectPalette(hdc,hPalette, FALSE );
		RealizePalette( hdc );
	}

	min_w = gr_screen.clip_width;
	if ( w < min_w ) min_w = w;

	min_h = gr_screen.clip_height;
	if ( h < min_h ) min_h = h;

	BitBlt(hdc,x,y,min_w,min_h,hDibDC,gr_screen.offset_x,gr_screen.offset_y,SRCCOPY);

	//StretchBlt( hdc, 0, 0, w, h, hDibDC, 0, 0, 640, 480, SRCCOPY );
	
	if ( hOldPalette ){	
		SelectPalette(hdc,hOldPalette, FALSE );
	}
}


// sets the clipping region & offset
void grx_set_clip(int x,int y,int w,int h)
{
	gr_screen.offset_x = x;
	gr_screen.offset_y = y;

	gr_screen.clip_left = 0;
	gr_screen.clip_right = w-1;

	gr_screen.clip_top = 0;
	gr_screen.clip_bottom = h-1;

	// check for sanity of parameters
	if ( gr_screen.clip_left+x < 0 ) {
		gr_screen.clip_left = -x;
	} else if ( gr_screen.clip_left+x > gr_screen.max_w-1 )	{
		gr_screen.clip_left = gr_screen.max_w-1-x;
	}
	if ( gr_screen.clip_right+x < 0 ) {
		gr_screen.clip_right = -x;
	} else if ( gr_screen.clip_right+x >= gr_screen.max_w-1 )	{
		gr_screen.clip_right = gr_screen.max_w-1-x;
	}

	if ( gr_screen.clip_top+y < 0 ) {
		gr_screen.clip_top = -y;
	} else if ( gr_screen.clip_top+y > gr_screen.max_h-1 )	{
		gr_screen.clip_top = gr_screen.max_h-1-y;
	}

	if ( gr_screen.clip_bottom+y < 0 ) {
		gr_screen.clip_bottom = -y;
	} else if ( gr_screen.clip_bottom+y > gr_screen.max_h-1 )	{
		gr_screen.clip_bottom = gr_screen.max_h-1-y;
	}

	gr_screen.clip_width = gr_screen.clip_right - gr_screen.clip_left + 1;
	gr_screen.clip_height = gr_screen.clip_bottom - gr_screen.clip_top + 1;
}

// resets the clipping region to entire screen
//
// should call this before gr_surface_flip() if you clipped some portion of the screen but still 
// want a full-screen display
void grx_reset_clip()
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


// Sets the current bitmap
void grx_set_bitmap( int bitmap_num, int alphablend_mode, int bitblt_mode, float alpha, int sx, int sy )
{
	gr_screen.current_alpha = alpha;
	gr_screen.current_alphablend_mode = alphablend_mode;
	gr_screen.current_bitblt_mode = bitblt_mode;
	gr_screen.current_bitmap = bitmap_num;
	gr_screen.current_bitmap_sx = sx;
	gr_screen.current_bitmap_sy = sy;
}


// clears entire clipping region to black.
void grx_clear()
{
	gr_lock();

	int i,w;
	ubyte *pDestBits;

	w = gr_screen.clip_right-gr_screen.clip_left+1;
	for (i=gr_screen.clip_top; i<=gr_screen.clip_bottom; i++)	{
		pDestBits = GR_SCREEN_PTR(ubyte,gr_screen.clip_left,i);
		memset( pDestBits, 0, w );
	}	

	gr_unlock();
}



void grx_start_frame()
{
}

void grx_stop_frame()
{
}

void gr_soft_fog_set(int fog_mode, int r, int g, int b, float near, float far)
{
}

void gr_soft_get_pixel(int x, int y, int *r, int *g, int *b)
{
}

void grx_fade_in(int instantaneous);
void grx_fade_out(int instantaneous);
void grx_flash(int r, int g, int b);

static ubyte *Gr_saved_screen = NULL;
static uint Gr_saved_screen_palette_checksum = 0;
static ubyte Gr_saved_screen_palette[768];

int gr8_save_screen()
{
	int i;
	gr_reset_clip();

	if (gr_screen.bits_per_pixel != 8) {
		mprintf(( "Save Screen only works in 8 bpp!\n" ));
		return -1;
	}

	if ( Gr_saved_screen )	{
		mprintf(( "Screen alread saved!\n" ));
		return -1;
	}

	Gr_saved_screen = (ubyte *)malloc( gr_screen.max_w*gr_screen.max_h );
	if (!Gr_saved_screen) {
		mprintf(( "Couldn't get memory for saved screen!\n" ));
		return -1;
	}

	Gr_saved_screen_palette_checksum = gr_palette_checksum;
	memcpy( Gr_saved_screen_palette, gr_palette, 768 );

	gr_lock();

	for (i=0; i<gr_screen.max_h; i++ )	{
		ubyte *dptr = GR_SCREEN_PTR(ubyte,0,i);
		memcpy( &Gr_saved_screen[gr_screen.max_w*i], dptr, gr_screen.max_w );
	}

	gr_unlock();

	return 0;
}


void gr8_restore_screen(int id)
{
	int i;
	gr_reset_clip();

	if ( !Gr_saved_screen )	{
		gr_clear();
		return;
	}

	if ( Gr_saved_screen_palette_checksum != gr_palette_checksum )	{
		// Palette changed! Remap the bitmap!
		ubyte xlat[256];
		for (i=0; i<256; i++ )	{
			xlat[i] = (ubyte)palette_find( Gr_saved_screen_palette[i*3+0], Gr_saved_screen_palette[i*3+1], Gr_saved_screen_palette[i*3+2] );
		}	

		for (i=0; i<gr_screen.max_h*gr_screen.max_w; i++ )	{
			Gr_saved_screen[i] = xlat[Gr_saved_screen[i]];
		}

		memcpy( Gr_saved_screen_palette, gr_palette, 768 );
		Gr_saved_screen_palette_checksum = gr_palette_checksum;
	}

	gr_lock();

	for (i=0; i<gr_screen.max_h; i++ )	{
		ubyte *dptr = GR_SCREEN_PTR(ubyte,0,i);
		memcpy( dptr, &Gr_saved_screen[gr_screen.max_w*i], gr_screen.max_w );
	}

	gr_unlock();
}


void gr8_free_screen(int id)
{
	if ( Gr_saved_screen )	{
		free( Gr_saved_screen );
		Gr_saved_screen = NULL;
	}
}

static int Gr8_dump_frames = 0;
static ubyte *Gr8_dump_buffer = NULL;
static int Gr8_dump_frame_number = 0;
static int Gr8_dump_frame_count = 0;
static int Gr8_dump_frame_count_max = 0;
static int Gr8_dump_frame_size = 0;


void gr8_dump_frame_start(int first_frame, int frames_between_dumps)
{
	if ( Gr8_dump_frames )	{
		Int3();		//  We're already dumping frames.  See John.
		return;
	}	
	Gr8_dump_frames = 1;
	Gr8_dump_frame_number = first_frame;
	Gr8_dump_frame_count = 0;
	Gr8_dump_frame_count_max = frames_between_dumps;
	Gr8_dump_frame_size = 640 * 480;
	
	if ( !Gr8_dump_buffer ) {
		int size = Gr8_dump_frame_count_max * Gr8_dump_frame_size;
		Gr8_dump_buffer = (ubyte *)malloc(size);
		if ( !Gr8_dump_buffer )	{
			Error(LOCATION, "Unable to malloc %d bytes for dump buffer", size );
		}
	}
}

// A hacked function to dump the frame buffer contents
void gr8_dump_screen_hack( void * dst )
{
	int i;

	gr_lock();
	for (i = 0; i < 480; i++)	{
		memcpy( (ubyte *)dst+(i*640), GR_SCREEN_PTR(ubyte,0,i), 640 );
	}
	gr_unlock();
}

void gr8_flush_frame_dump()
{
	ubyte *buffer[480];
	char filename[MAX_PATH_LEN], *movie_path = "";

	int i;
	for (i = 0; i < Gr8_dump_frame_count; i++) {
		int j;

		for ( j = 0; j < 480; j++ )
			buffer[j] = Gr8_dump_buffer+(i*Gr8_dump_frame_size)+(j*640);

		sprintf(filename, NOX("%sfrm%04d"), movie_path, Gr8_dump_frame_number );
		pcx_write_bitmap(filename, 640, 480, buffer, gr_palette);
		Gr8_dump_frame_number++;
	}
}

void gr8_dump_frame()
{
	// A hacked function to dump the frame buffer contents
	gr8_dump_screen_hack( Gr8_dump_buffer+(Gr8_dump_frame_count*Gr8_dump_frame_size) );

	Gr8_dump_frame_count++;

	if ( Gr8_dump_frame_count == Gr8_dump_frame_count_max ) {
		gr8_flush_frame_dump();
		Gr8_dump_frame_count = 0;
	}
}

void grx_get_region(int front, int w, int h, ubyte *data)
{
}

// resolution checking
int gr_soft_supports_res_ingame(int res)
{
	return 1;
}

int gr_soft_supports_res_interface(int res)
{
	return 1;
}

void gr8_dump_frame_stop()
{
	if ( !Gr8_dump_frames )	{
		Int3();		//  We're not dumping frames.  See John.
		return;
	}	

	// dump any remaining frames
	gr8_flush_frame_dump();
	
	Gr8_dump_frames = 0;
	if ( Gr8_dump_buffer )	{
		free(Gr8_dump_buffer);
		Gr8_dump_buffer = NULL;
	}
}

void gr_soft_set_cull(int cull)
{
}

// cross fade
void gr_soft_cross_fade(int bmap1, int bmap2, int x1, int y1, int x2, int y2, float pct)
{
	if ( pct <= 50 )	{
		gr_set_bitmap(bmap1);
		gr_bitmap(x1, y1);
	} else {
		gr_set_bitmap(bmap2);
		gr_bitmap(x2, y2);
	}	
}

// filter
void gr_soft_filter_set(int filter)
{
}

// tcache
int gr_soft_tcache_set(int bitmap_id, int bitmap_type, float *u_ratio, float *v_ratio, int fail_on_full = 0, int sx = -1, int sy = -1, int force = 0 )
{
	return 1;
}

// clear color
void gr_soft_set_clear_color(int r, int g, int b)
{
}

extern uint Gr_signature;

//extern void gr_set_palette_internal(char *name, ubyte *pal);	

void gr8_set_gamma(float gamma)
{
	Gr_gamma = gamma;
	Gr_gamma_int = int(Gr_gamma*100);

	// Create the Gamma lookup table
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

//	ubyte new_pal[768];
//	if ( gr_screen.bits_per_pixel!=8 )	return;
//
//	for (i=0; i<768; i++ )	{
//		new_pal[i] = ubyte(Gr_gamma_lookup[gr_palette[i]]);
//	}
//	grx_change_palette( new_pal );

	gr_screen.signature = Gr_signature++;
}

void gr_soft_init()
{
//	int i;
	HWND hwnd = (HWND)os_get_window();
	
	// software mode only supports 640x480
	Assert(gr_screen.res == GR_640);
	if(gr_screen.res != GR_640){
		gr_screen.res = GR_640;
		gr_screen.max_w = 640;
		gr_screen.max_h = 480;
	}

	// Prepare the window to go full screen
	if ( hwnd )	{
		DWORD style, exstyle;
		RECT		client_rect;

		exstyle = 0;
		style = WS_CAPTION | WS_SYSMENU;
		
		//	Create Game Window
		client_rect.left = client_rect.top = 0;
		client_rect.right = gr_screen.max_w;
		client_rect.bottom = gr_screen.max_h;
		AdjustWindowRect(&client_rect,style,FALSE);

		RECT work_rect;
		SystemParametersInfo( SPI_GETWORKAREA, 0, &work_rect, 0 );
		int x = work_rect.left + (( work_rect.right - work_rect.left )-(client_rect.right - client_rect.left))/2;
		int y = work_rect.top;
		if ( x < work_rect.left ) {
			x = work_rect.left;
		}
		int WinX = x;
		int WinY = y;
		int WinW = client_rect.right - client_rect.left;
		int WinH = client_rect.bottom - client_rect.top;

		ShowWindow(hwnd, SW_SHOWNORMAL );
		SetWindowLong( hwnd, GWL_STYLE, style );
		SetWindowLong( hwnd, GWL_EXSTYLE, exstyle );
		SetWindowPos( hwnd, HWND_NOTOPMOST, WinX, WinY, WinW, WinH, SWP_SHOWWINDOW );
		SetActiveWindow(hwnd);
		SetForegroundWindow(hwnd);
	}

	Palette_flashed = 0;
	Palette_flashed_last_frame = 0;

	gr_screen.bits_per_pixel = 8;
	gr_screen.bytes_per_pixel = 1;

	gr_buffer_create( gr_screen.max_w, gr_screen.max_h, gr_screen.bits_per_pixel );

	gr_screen.offscreen_buffer_base = lpDibBits;

	gr_screen.rowsize = DibInfo.Header.biWidth*((gr_screen.bits_per_pixel+7)/8);
	Assert( DibInfo.Header.biWidth == gr_screen.max_w );

	if (DibInfo.Header.biHeight > 0)	{
		// top down 
		gr_screen.offscreen_buffer = (void *)((uint)gr_screen.offscreen_buffer_base + (gr_screen.max_h - 1) * gr_screen.rowsize);
		gr_screen.rowsize *= -1;
	} else {
		// top up
		gr_screen.offscreen_buffer = gr_screen.offscreen_buffer_base;
	}

	grx_init_alphacolors();

	gr_screen.gf_flip = grx_flip;
	gr_screen.gf_flip_window = grx_flip_window;
	gr_screen.gf_set_clip = grx_set_clip;
	gr_screen.gf_reset_clip = grx_reset_clip;
	gr_screen.gf_set_font = grx_set_font;
	gr_screen.gf_set_color = grx_set_color;
	gr_screen.gf_set_bitmap = grx_set_bitmap;
	gr_screen.gf_create_shader = grx_create_shader;
	gr_screen.gf_set_shader = grx_set_shader;
	gr_screen.gf_clear = grx_clear;
	// gr_screen.gf_bitmap = grx_bitmap;
	// ]gr_screen.gf_bitmap_ex = grx_bitmap_ex;

	gr_screen.gf_aabitmap = grx_aabitmap;
	gr_screen.gf_aabitmap_ex = grx_aabitmap_ex;

	gr_screen.gf_rect = grx_rect;
	gr_screen.gf_shade = gr8_shade;
	gr_screen.gf_string = gr8_string;
	gr_screen.gf_circle = gr8_circle;

	gr_screen.gf_line = gr8_line;
	gr_screen.gf_aaline = gr8_aaline;
	gr_screen.gf_pixel = gr8_pixel;
	gr_screen.gf_scaler = gr8_scaler;
	gr_screen.gf_aascaler = gr8_aascaler;
	gr_screen.gf_tmapper = grx_tmapper;

	gr_screen.gf_gradient = gr8_gradient;

	gr_screen.gf_set_palette = grx_set_palette;
	gr_screen.gf_get_color = grx_get_color;
	gr_screen.gf_init_color = grx_init_color;
	gr_screen.gf_init_alphacolor = grx_init_alphacolor;
	gr_screen.gf_set_color_fast = grx_set_color_fast;
	gr_screen.gf_print_screen = grx_print_screen;
	gr_screen.gf_start_frame = grx_start_frame;
	gr_screen.gf_stop_frame = grx_stop_frame;

	gr_screen.gf_fade_in = grx_fade_in;
	gr_screen.gf_fade_out = grx_fade_out;
	gr_screen.gf_flash = grx_flash;


	// Retrieves the zbuffer mode.
	gr_screen.gf_zbuffer_get = gr8_zbuffer_get;
	gr_screen.gf_zbuffer_set = gr8_zbuffer_set;
	gr_screen.gf_zbuffer_clear = gr8_zbuffer_clear;

	gr_screen.gf_save_screen = gr8_save_screen;
	gr_screen.gf_restore_screen = gr8_restore_screen;
	gr_screen.gf_free_screen = gr8_free_screen;

	// Screen dumping stuff
	gr_screen.gf_dump_frame_start = gr8_dump_frame_start;
	gr_screen.gf_dump_frame_stop = gr8_dump_frame_stop;
	gr_screen.gf_dump_frame = gr8_dump_frame;

	// Gamma stuff
	gr_screen.gf_set_gamma = gr8_set_gamma;

	// Lock/unlock stuff
	gr_screen.gf_lock = gr_soft_lock;
	gr_screen.gf_unlock = gr_soft_unlock;

	// region
	gr_screen.gf_get_region = grx_get_region;

	// fog stuff
	gr_screen.gf_fog_set = gr_soft_fog_set;	

	// pixel get
	gr_screen.gf_get_pixel = gr_soft_get_pixel;

	// poly culling
	gr_screen.gf_set_cull = gr_soft_set_cull;

	// cross fade
	gr_screen.gf_cross_fade = gr_soft_cross_fade;

	// filter
	gr_screen.gf_filter_set = gr_soft_filter_set;

	// tcache set
	gr_screen.gf_tcache_set = gr_soft_tcache_set;

	// set clear color
	gr_screen.gf_set_clear_color = gr_soft_set_clear_color;

	gr_reset_clip();
	gr_clear();
	gr_flip();
}

void gr_soft_force_windowed()
{
}

void gr_soft_cleanup()
{
	if (Gr_soft_inited) {
		gr_buffer_release();
		Gr_soft_inited = 0;
	}
}

void grx_change_palette( ubyte * new_pal )
{
	if ( hPalette )	{
		if (hDibDC)
			SelectPalette( hDibDC, hOldPalette, FALSE );
		if (!DeleteObject(hPalette))
			Int3();
		hPalette = NULL;
	}

	hPalette = gr_create_palette_256(new_pal);	// All 256 mapped one-to-one, but BLT's are slow.

	if ( hDibDC )	{
		int i; 
		for (i=0; i<256; i++ )	{
			DibInfo.Colors.aColors[i].rgbRed = new_pal[i*3+0];
			DibInfo.Colors.aColors[i].rgbGreen = new_pal[i*3+1];
			DibInfo.Colors.aColors[i].rgbBlue = new_pal[i*3+2];
			DibInfo.Colors.aColors[i].rgbReserved = 0;
		}

		hOldPalette = SelectPalette( hDibDC, hPalette, FALSE );
		SetDIBColorTable( hDibDC, 0, 256, DibInfo.Colors.aColors );
	}
}

void grx_flash( int r, int g, int b )
{
	int t,i;
	ubyte new_pal[768];

	if ( (r==0) && (g==0) && (b==0) )	{
		return;
	}

	Palette_flashed++;

	for (i=0; i<256; i++ )	{
		t = gr_palette[i*3+0] + r;
		if ( t < 0 ) t = 0; else if (t>255) t = 255;
		new_pal[i*3+0] = (ubyte)t;

		t = gr_palette[i*3+1] + g;
		if ( t < 0 ) t = 0; else if (t>255) t = 255;
		new_pal[i*3+1] = (ubyte)t;

		t = gr_palette[i*3+2] + b;
		if ( t < 0 ) t = 0; else if (t>255) t = 255;
		new_pal[i*3+2] = (ubyte)t;
	}

	grx_change_palette( new_pal );
}


static int gr_palette_faded_out = 0;

#define FADE_TIME (F1_0/4)		// How long to fade out

void grx_fade_out(int instantaneous)	
{
#ifndef HARDWARE_ONLY
	int i;
	ubyte new_pal[768];

	if (!gr_palette_faded_out) {

		if ( !instantaneous )	{
	
			int count = 0;
			fix start_time, stop_time, t1;

			start_time = timer_get_fixed_seconds();
			t1 = 0;

			do {
				for (i=0; i<768; i++ )	{		
					int c = (gr_palette[i]*(FADE_TIME-t1))/FADE_TIME;
					if (c < 0 )
						c = 0;
					else if ( c > 255 )
						c = 255;
			
					new_pal[i] = (ubyte)c;
				}
				grx_change_palette( new_pal );
				gr_flip();
				count++;

				t1 = timer_get_fixed_seconds() - start_time;

			} while ( (t1 < FADE_TIME) && (t1>=0) );		// Loop as long as time not up and timer hasn't rolled

			stop_time = timer_get_fixed_seconds();

			mprintf(( "Took %d frames (and %.1f secs) to fade out\n", count, f2fl(stop_time-start_time) ));
		
		}
		gr_palette_faded_out = 1;
	}

	gr_reset_clip();
	gr_clear();
	gr_flip();
	memset( new_pal, 0, 768 );
	grx_change_palette( new_pal );
#else
	Int3();
#endif
}


void grx_fade_in(int instantaneous)	
{
#ifndef HARDWARE_ONLY
	int i;
	ubyte new_pal[768];

	if (gr_palette_faded_out)	{

		if ( !instantaneous )	{
			int count = 0;
			fix start_time, stop_time, t1;

			start_time = timer_get_fixed_seconds();
			t1 = 0;

			do {
				for (i=0; i<768; i++ )	{		
					int c = (gr_palette[i]*t1)/FADE_TIME;
					if (c < 0 )
						c = 0;
					else if ( c > 255 )
						c = 255;
			
					new_pal[i] = (ubyte)c;
				}
				grx_change_palette( new_pal );
				gr_flip();
				count++;

				t1 = timer_get_fixed_seconds() - start_time;

			} while ( (t1 < FADE_TIME) && (t1>=0) );		// Loop as long as time not up and timer hasn't rolled

			stop_time = timer_get_fixed_seconds();

			mprintf(( "Took %d frames (and %.1f secs) to fade in\n", count, f2fl(stop_time-start_time) ));
		}
		gr_palette_faded_out = 0;
	}

	memcpy( new_pal, gr_palette, 768 );
	grx_change_palette( new_pal );
#else 
	Int3();
#endif
}


