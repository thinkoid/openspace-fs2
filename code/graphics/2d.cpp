/*
 * Copyright (C) Volition, Inc. 1999.  All rights reserved.
 *
 * All source code herein is the property of Volition, Inc. You may not sell 
 * or otherwise commercially exploit the source or things you created based on the 
 * source.
 *
*/ 

#include <windows.h>
#include <windowsx.h>

#include "osapi.h"
#include "2d.h"
#include "3d.h"
#include "bmpman.h"
#include "palman.h"
#include "font.h"
#include "grinternal.h"
#include "systemvars.h"
#include "cmdline.h"

// 3dnow stuff
// #include "amd3d.h"

// Includes for different rendering systems
#include "grsoft.h"
#include "grd3d.h"
#include "grglide.h"
#include "gropengl.h"
#include "grdirectdraw.h"

screen gr_screen;

color_gun Gr_red, Gr_green, Gr_blue, Gr_alpha;
color_gun Gr_t_red, Gr_t_green, Gr_t_blue, Gr_t_alpha;
color_gun Gr_ta_red, Gr_ta_green, Gr_ta_blue, Gr_ta_alpha;
color_gun *Gr_current_red, *Gr_current_green, *Gr_current_blue, *Gr_current_alpha;


ubyte Gr_original_palette[768];		// The palette 
ubyte Gr_current_palette[768];
char Gr_current_palette_name[128] = NOX("none");

// cursor stuff
int Gr_cursor = -1;
int Web_cursor_bitmap = -1;

int Gr_inited = 0;

// cpu types
int Gr_cpu = 0;	
int Gr_amd3d = 0;
int Gr_katmai = 0;
int Gr_mmx = 0;

uint Gr_signature = 0;

float Gr_gamma = 1.8f;
int Gr_gamma_int = 180;
int Gr_gamma_lookup[256];

void gr_close()
{
	if ( !Gr_inited )	return;

	palette_flush();

	switch( gr_screen.mode )	{
	case GR_SOFTWARE:		
		gr_soft_cleanup();
		break;
	case GR_DIRECTDRAW:
		Int3();
		gr_directdraw_cleanup();
		break;
	case GR_DIRECT3D:		
		gr_d3d_cleanup();
		break;
	case GR_GLIDE:
		gr_glide_cleanup();
		break;
	case GR_OPENGL:
		Int3();
		gr_opengl_cleanup();
		break;
	default:
		Int3();		// Invalid graphics mode
	}

	gr_font_close();

	Gr_inited = 0;
}

//XSTR:OFF
DCF(gr,"Changes graphics mode")
{
#ifndef HARDWARE_ONLY
	int mode = gr_screen.mode;

	if ( Dc_command )	{
		dc_get_arg(ARG_STRING);
		
		if ( !strcmp( Dc_arg, "a"))	{
			Int3();
			mode = GR_SOFTWARE;
		} else if ( !strcmp( Dc_arg, "b"))	{
			Int3();
			mode = GR_DIRECTDRAW;
		} else if ( !strcmp( Dc_arg, "d"))	{
			mode = GR_DIRECT3D;
		} else if ( !strcmp( Dc_arg, "g"))	{
			mode = GR_GLIDE;
		} else if ( !strcmp( Dc_arg, "o"))	{
			Int3();
			mode = GR_OPENGL;
		} else {
			// print usage, not stats
			Dc_help = 1;
		}

		/*
		if ( mode != gr_screen.mode )	{
			dc_printf( "Setting new video mode...\n" );
			int errcode = gr_init( gr_screen.max_w, gr_screen.max_h, mode );
			if (errcode)	{
				dc_printf( "Error %d.  Graphics unchanged.\n", errcode );
			}
		}
		*/
	}

	if ( Dc_help )	{
		dc_printf( "Usage: gr mode\n" );
		dc_printf( "The options can be:\n" );
		dc_printf( "Macros:  A=software win32 window (obsolete)\n" );
		dc_printf( "         B=software directdraw fullscreen (obsolete)\n" );
		dc_printf( "         D=Direct3d\n" );
		dc_printf( "         G=Glide\n" );
		dc_printf( "         O=OpenGl (obsolete)\n" );
		Dc_status = 0;	// don't print status if help is printed.  Too messy.
	}

	if ( Dc_status )	{
		switch( gr_screen.mode )	{
		case GR_SOFTWARE:
			Int3();
			dc_printf( "Win32 software windowed\n" );
			break;
		case GR_DIRECTDRAW:
			Int3();
			dc_printf( "DirectDraw software windowed\n" );
			break;
		case GR_DIRECT3D:
			dc_printf( "Direct3D\n" );
			break;
		case GR_GLIDE:
			dc_printf( "3Dfx Glide\n" );
			break;
		case GR_OPENGL:
			Int3();
			dc_printf( "OpenGl\n" );
			break;
		default:
			Int3();		// Invalid graphics mode
		}
	}
#endif
}
//XSTR:ON

// set screen clear color
DCF(clear_color, "set clear color r, g, b")
{
	int r, g, b;

	dc_get_arg(ARG_INT);
	r = Dc_arg_int;
	dc_get_arg(ARG_INT);
	g = Dc_arg_int;
	dc_get_arg(ARG_INT);
	b = Dc_arg_int;

	// set the color
	gr_set_clear_color(r, g, b);
}

void gr_set_palette_internal( char *name, ubyte * palette, int restrict_font_to_128 )
{
	if ( palette == NULL )	{
		// Create a default palette
		int r,g,b,i;
		i = 0;
				
		for (r=0; r<6; r++ )	
			for (g=0; g<6; g++ )	
				for (b=0; b<6; b++ )		{
					Gr_current_palette[i*3+0] = (unsigned char)(r*51);
					Gr_current_palette[i*3+1] = (unsigned char)(g*51);
					Gr_current_palette[i*3+2] = (unsigned char)(b*51);
					i++;
				}
		for ( i=216;i<256; i++ )	{
			Gr_current_palette[i*3+0] = (unsigned char)((i-216)*6);
			Gr_current_palette[i*3+1] = (unsigned char)((i-216)*6);
			Gr_current_palette[i*3+2] = (unsigned char)((i-216)*6);
		}
		memmove( Gr_original_palette, Gr_current_palette, 768 );
	} else {
		memmove( Gr_original_palette, palette, 768 );
		memmove( Gr_current_palette, palette, 768 );
	}

//	mprintf(("Setting new palette\n" ));

	if ( Gr_inited )	{
		if (gr_screen.gf_set_palette)	{
			(*gr_screen.gf_set_palette)(Gr_current_palette, restrict_font_to_128 );

			// Since the palette set code might shuffle the palette,
			// reload it into the source palette
			if ( palette )
				memmove( palette, Gr_current_palette, 768 );
		}

		// Update Palette Manager tables
		memmove( gr_palette, Gr_current_palette, 768 );
		palette_update(name, restrict_font_to_128);
	}
}


void gr_set_palette( char *name, ubyte * palette, int restrict_font_to_128 )
{
	char *p;
	palette_flush();
	strcpy( Gr_current_palette_name, name );
	p = strchr( Gr_current_palette_name, '.' );
	if ( p ) *p = 0;
	gr_screen.signature = Gr_signature++;
	gr_set_palette_internal( name, palette, restrict_font_to_128 );
}


//void gr_test();

#define CPUID _asm _emit 0fh _asm _emit 0a2h

// -----------------------------------------------------------------------
// Returns cpu type.
void gr_detect_cpu(int *cpu, int *mmx, int *amd3d, int *katmai )
{
	DWORD RegEDX;
	DWORD RegEAX;

	// Set defaults
	*cpu = 0;
	*mmx = 0;
	*amd3d = 0;
	*katmai = 0;

	char cpu_vender[16];
	memset( cpu_vender, 0, sizeof(cpu_vender) );
		
  _asm {

		// Check for prescence of 
		push	eax
		push	ebx
		push	ecx
		push	edx

		pushfd			// get extended flags
		pop	eax
		mov	ebx, eax		// save current flags
		xor	eax, 200000h	// toggle bit 21
		push	eax			// push new flags on stack
		popfd					// flags updated now in flags
		pushfd			// get extended flags
		pop	eax		// store extended flags in eax
		xor	eax, ebx	// if bit 21 r/w then eax <> 0
		je		no_cpuid		

		mov	eax, 0		// setup CPUID to return vender id
      CPUID           // code bytes = 0fh,  0a2h
		mov	DWORD PTR cpu_vender[0], ebx
		mov	DWORD PTR cpu_vender[4], edx
		mov	DWORD PTR cpu_vender[8], ecx
		
      mov eax, 1      // setup CPUID to return features

      CPUID           // code bytes = 0fh,  0a2h

		mov RegEAX, eax	// family, etc returned in eax
      mov RegEDX, edx	// features returned in edx
		jmp	done_checking_cpuid


no_cpuid:
		mov RegEAX, 4<<8	// family, etc returned in eax
      mov RegEDX, 0		// features returned in edx

done_checking_cpuid:								
		pop	edx
		pop	ecx
		pop	ebx
		pop	eax

	}
	


	//RegEAX	.  Bits 11:8 is family
	*cpu = (RegEAX >>8) & 0xF;

	if ( *cpu < 5 )	{
		*cpu = 4;								// processor does not support CPUID
		*mmx = 0;
	}

	//RegEAX	.  Bits 11:8 is family
	*cpu = (RegEAX >>8) & 0xF;

	// Check for MMX
	BOOL retval = TRUE;
   if (RegEDX & 0x800000)               // bit 23 is set for MMX technology
   {

           __try { _asm emms }          // try executing an MMX instruction "emms"

           __except(EXCEPTION_EXECUTE_HANDLER) { retval = FALSE; }

   } else {
		retval = FALSE;
	}
	if ( retval )	{
		*mmx = 1;			// processor supports CPUID but does not support MMX technology
	}

	// Check for Katmai
   if (RegEDX & (1<<25) )               // bit 25 is set for Katmai technology
   {
		*katmai = 1;
   }

	// Check for Amd 3dnow
	/*
	if ( !stricmp( cpu_vender, NOX("AuthenticAMD")) )	{

		_asm {
			mov eax, 0x80000000      // setup CPUID to return extended number of functions

			CPUID           // code bytes = 0fh,  0a2h

			mov RegEAX, eax	// highest extended function value
		}

		if ( RegEAX > 0x80000000 )	{

			_asm {
				mov eax, 0x80000001      // setup CPUID to return extended flags

				CPUID           // code bytes = 0fh,  0a2h

				mov RegEAX, eax	// family, etc returned in eax
				mov RegEDX, edx	// flags in edx
			}

			if (RegEDX & 0x80000000)               // bit 31 is set for AMD-3D technology
			{
				// try executing some 3Dnow instructions
				__try { 

					float x = (float)1.25;            
					float y = (float)1.25;            
					float z;                      

					_asm {
						movd		mm1, x
						movd		mm2, y                  
						PFMUL(AMD_M1, AMD_M2);               
						movd		z, mm1
						femms
						emms
					}

					int should_be_156 = int(z*100);

					if ( should_be_156 == 156 )	{
						*amd3d = 1;
					}

				}          

				__except(EXCEPTION_EXECUTE_HANDLER) { }
			}

		}		
	}
	*/
}

// --------------------------------------------------------------------------

int gr_init(int res, int mode, int depth, int fred_x, int fred_y)
{
	int first_time = 0;
	int max_w, max_h;

	gr_detect_cpu(&Gr_cpu, &Gr_mmx, &Gr_amd3d, &Gr_katmai );

	mprintf(( "GR_CPU: Family %d, MMX=%s\n", Gr_cpu, (Gr_mmx?"Yes":"No") ));
	
//	gr_test();

	if ( !Gr_inited )	
		atexit(gr_close);

	// If already inited, shutdown the previous graphics
	if ( Gr_inited )	{
		switch( gr_screen.mode )	{
		case GR_SOFTWARE:			
			gr_soft_cleanup();
			break;
		case GR_DIRECTDRAW:
			Int3();
			gr_directdraw_cleanup();
			break;
		case GR_DIRECT3D:			
			gr_d3d_cleanup();
			break;
		case GR_GLIDE:
			gr_glide_cleanup();
			break;
		case GR_OPENGL:
			Int3();
			gr_opengl_cleanup();
			break;
		default:
			Int3();		// Invalid graphics mode
		}
	} else {
		first_time = 1;
	}

#if defined(HARDWARE_ONLY)
	if(!Fred_running && !Pofview_running && !Nebedit_running && !Is_standalone){
		if((mode != GR_GLIDE) && (mode != GR_DIRECT3D)){
			mprintf(("Forcing glide startup!\n"));
			mode = GR_GLIDE;
		}	
	}
#endif

	D3D_enabled = 0;
	Gr_inited = 1;

	max_w = -1;
	max_h = -1;
	if(!Fred_running && !Pofview_running){
		// set resolution based on the res type
		switch(res){
		case GR_640:
			max_w = 640;
			max_h = 480;
			break;

		case GR_1024:
			max_w = 1024;
			max_h = 768;
			break;

		default :
			Int3();
		}
	} else {		
		max_w = fred_x;
		max_h = fred_y;
	}

	// Make w a multiple of 8
	max_w = ( max_w / 8 )*8;
	if ( max_w < 8 ) max_w = 8;
	if ( max_h < 8 ) max_h = 8;

	memset( &gr_screen, 0, sizeof(screen) );

	gr_screen.signature = Gr_signature++;
	gr_screen.mode = mode;
	gr_screen.res = res;	
	gr_screen.max_w = max_w;
	gr_screen.max_h = max_h;
	gr_screen.aspect = 1.0f;			// Normal PC screen
	gr_screen.offset_x = 0;
	gr_screen.offset_y = 0;
	gr_screen.clip_left = 0;
	gr_screen.clip_top = 0;
	gr_screen.clip_right = gr_screen.max_w - 1;
	gr_screen.clip_bottom = gr_screen.max_h - 1;
	gr_screen.clip_width = gr_screen.max_w;
	gr_screen.clip_height = gr_screen.max_h;

	switch( gr_screen.mode )	{
		case GR_SOFTWARE:
			Assert(Fred_running || Pofview_running || Is_standalone || Nebedit_running);
			gr_soft_init();
			break;
		case GR_DIRECTDRAW:
			Int3();
			gr_directdraw_init();
			break;
		case GR_DIRECT3D:
			// we only care about possible 32 bit stuff here
			Cmdline_force_32bit = 0;
			if(depth == 32){
				Cmdline_force_32bit = 1;
			} 

			gr_d3d_init();

			// bad startup - stupid D3D
			extern int D3D_inited;
			if(!D3D_inited){
				Gr_inited = 0;
				return 1;
			}

			break;
		case GR_GLIDE:
			// if we're in high-res. force polygon interface
			if(gr_screen.res == GR_1024){
				Gr_bitmap_poly = 1;
			}
			gr_glide_init();
			break;
		case GR_OPENGL:
			Int3();
			gr_opengl_init();
			break;
		default:
			Int3();		// Invalid graphics mode
	}

	memmove( Gr_current_palette, Gr_original_palette, 768 );
	gr_set_palette_internal(Gr_current_palette_name, Gr_current_palette,0);	

	gr_set_gamma(Gr_gamma);

	if ( Gr_cursor == -1 ){
		Gr_cursor = bm_load( "cursor" );
	}

	// load the web pointer cursor bitmap
	if (Web_cursor_bitmap < 0)	{
		int nframes;						// used to pass, not really needed (should be 1)
		Web_cursor_bitmap = bm_load_animation("cursorweb", &nframes);
		Assert(Web_cursor_bitmap >= 0);		// if bitmap didnt load, thats not good (this is protected for in release tho)
	}

	gr_set_color(0,0,0);

	gr_set_clear_color(0, 0, 0);

	// Call some initialization functions
	gr_set_shader(NULL);

	return 0;
}

void gr_force_windowed()
{
	if ( !Gr_inited )	return;

	switch( gr_screen.mode )	{
		case GR_SOFTWARE:
			{				
				extern void gr_soft_force_windowed();
				gr_soft_force_windowed();
			}
			break;
		case GR_DIRECTDRAW:
			{
				Int3();
				extern void gr_directdraw_force_windowed();
				gr_directdraw_force_windowed();
			}
			break;
		case GR_DIRECT3D:
			break;
		case GR_GLIDE:
			{
				extern void gr_glide_force_windowed();
				gr_glide_force_windowed();
			}
			break;
		case GR_OPENGL:
			Int3();
			break;

		default:
			Int3();		// Invalid graphics mode
	}

	if ( Os_debugger_running )
		Sleep(1000);		

}

void gr_activate(int active)
{
	if ( !Gr_inited ) return;

	switch( gr_screen.mode )	{
		case GR_SOFTWARE:
			{				
				extern void gr_soft_activate(int active);
				gr_soft_activate(active);
				return;
			}
			break;
		case GR_DIRECTDRAW:
			{
				Int3();
				extern void gr_dd_activate(int active);
				gr_dd_activate(active);
				return;
			}
			break;
		case GR_DIRECT3D:
			{	
				extern void gr_d3d_activate(int active);
				gr_d3d_activate(active);
				return;
			}
			break;
		case GR_GLIDE:
			{
				extern void gr_glide_activate(int active);
				gr_glide_activate(active);
				return;
			}
			break;
		case GR_OPENGL:
			Int3();
			break;
		default:
			Int3();		// Invalid graphics mode
	}

}

// -----------------------------------------------------------------------
// gr_set_cursor_bitmap()
//
// Set the bitmap for the mouse pointer.  This is called by the animating mouse
// pointer code.
//
// The lock parameter just locks basically disables the next call of this function that doesnt
// have an unlock feature.  If adding in more cursor-changing situations, be aware of
// unexpected results. You have been warned.
//
// TODO: investigate memory leak of original Gr_cursor bitmap when this is called
void gr_set_cursor_bitmap(int n, int lock)
{
	static int locked = 0;			
	Assert(n >= 0);

	if (!locked || (lock == GR_CURSOR_UNLOCK)) {
		Gr_cursor = n;
	} else {
		locked = 0;
	}

	if (lock == GR_CURSOR_LOCK) {
		locked = 1;
	}
}

// retrieves the current bitmap
// used in UI_GADGET to save/restore current cursor state
int gr_get_cursor_bitmap()
{
	return Gr_cursor;
}


int Gr_bitmap_poly = 0;
DCF(bmap, "")
{
	Gr_bitmap_poly = !Gr_bitmap_poly;

	if(Gr_bitmap_poly){
		dc_printf("Using poly bitmaps\n");
	} else {
		dc_printf("Using LFB bitmaps\n");
	}
}

// new bitmap functions
void gr_bitmap(int x, int y)
{
	int section_x, section_y;	
	int x_line, y_line;
	int w, h;

	// d3d and glide support texture poly shiz
	if(((gr_screen.mode == GR_DIRECT3D) || (gr_screen.mode == GR_GLIDE)) && Gr_bitmap_poly){		
		int idx, s_idx;
		// float u_scale, v_scale;
		bitmap_section_info *sections;			

		// render all sections
		bm_get_info(gr_screen.current_bitmap, &w, &h, NULL, NULL, NULL, &sections);
		y_line = 0;
		section_y = 0;
		for(idx=0; idx<sections->num_y; idx++){
			x_line = 0;
			for(s_idx=0; s_idx<sections->num_x; s_idx++){
				// get the section as a texture in vram					
				gr_set_bitmap(gr_screen.current_bitmap, gr_screen.current_alphablend_mode, gr_screen.current_bitblt_mode, gr_screen.current_alpha, s_idx, idx);

				// determine the width and height of this section
				bm_get_section_size(gr_screen.current_bitmap, s_idx, idx, &section_x, &section_y);

				// draw as a poly
				g3_draw_2d_poly_bitmap(x + x_line, y + y_line, section_x, section_y, TMAP_FLAG_BITMAP_SECTION);
				x_line += section_x;
			}
			y_line += section_y;
		}

		// done. whee!
		return;
	}			

	// old school bitmaps
	switch(gr_screen.mode){
	case GR_SOFTWARE:
	case GR_DIRECTDRAW:
		grx_bitmap(x, y);
		break;

	case GR_DIRECT3D:
		gr_d3d_bitmap(x, y);
		break;
	
	case GR_GLIDE:		
		gr_glide_bitmap(x, y);		
		break;

	case GR_OPENGL:
		gr_opengl_bitmap(x, y);
		break;
	}
}

void gr_bitmap_ex(int x, int y, int w, int h, int sx, int sy)
{
	switch(gr_screen.mode){
	case GR_SOFTWARE:
	case GR_DIRECTDRAW:
		grx_bitmap_ex(x, y, w, h, sx, sy);
		break;

	case GR_DIRECT3D:
		gr_d3d_bitmap_ex(x, y, w, h, sx, sy);
		break;

	case GR_GLIDE:
		gr_glide_bitmap_ex(x, y, w, h, sx, sy);
		break;

	case GR_OPENGL:
		gr_opengl_bitmap_ex(x, y, w, h, sx, sy);
	}
}

// given endpoints, and thickness, calculate coords of the endpoint
void gr_pline_helper(vector *out, vector *in1, vector *in2, int thickness)
{
	vector slope;	

	// slope of the line	
	if(vm_vec_same(in1, in2)){
		slope = vmd_zero_vector;
	} else {
		vm_vec_sub(&slope, in2, in1);
		float temp = -slope.x;
		slope.x = slope.y;
		slope.y = temp;
		vm_vec_normalize(&slope);
	}

	// get the points		
	vm_vec_scale_add(out, in1, &slope, (float)thickness);
}

// special function for drawing polylines. this function is specifically intended for
// polylines where each section is no more than 90 degrees away from a previous section.
// Moreover, it is _really_ intended for use with 45 degree angles. 
void gr_pline_special(vector **pts, int num_pts, int thickness)
{				
	vector s1, s2, e1, e2, dir;
	vector last_e1, last_e2;
	vertex v[4];
	vertex *verts[4] = {&v[0], &v[1], &v[2], &v[3]};
	int saved_zbuffer_mode, idx;		
	int started_frame = 0;

	// Assert(0);

	// if we have less than 2 pts, bail
	if(num_pts < 2){
		return;
	}	

	extern int G3_count;
	if(G3_count == 0){
		g3_start_frame(1);		
		started_frame = 1;
	}

	// turn off zbuffering	
	saved_zbuffer_mode = gr_zbuffer_get();
	gr_zbuffer_set(GR_ZBUFF_NONE);	

	// turn off culling
	gr_set_cull(0);

	// draw each section
	last_e1 = vmd_zero_vector;
	last_e2 = vmd_zero_vector;
	for(idx=0; idx<num_pts-1; idx++){		
		// get the start and endpoints		
		s1 = *pts[idx];														// start 1 (on the line)
		gr_pline_helper(&s2, pts[idx], pts[idx+1], thickness);	// start 2
		e1 = *pts[idx+1];														// end 1 (on the line)
		vm_vec_sub(&dir, pts[idx+1], pts[idx]);		
		vm_vec_add(&e2, &s2, &dir);										// end 2
		
		// stuff coords		
		v[0].sx = (float)ceil(s1.x);
		v[0].sy = (float)ceil(s1.y);	
		v[0].sw = 0.0f;
		v[0].u = 0.5f;
		v[0].v = 0.5f;
		v[0].flags = PF_PROJECTED;
		v[0].codes = 0;
		v[0].r = gr_screen.current_color.red;
		v[0].g = gr_screen.current_color.green;
		v[0].b = gr_screen.current_color.blue;

		v[1].sx = (float)ceil(s2.x);
		v[1].sy = (float)ceil(s2.y);	
		v[1].sw = 0.0f;
		v[1].u = 0.5f;
		v[1].v = 0.5f;
		v[1].flags = PF_PROJECTED;
		v[1].codes = 0;
		v[1].r = gr_screen.current_color.red;
		v[1].g = gr_screen.current_color.green;
		v[1].b = gr_screen.current_color.blue;

		v[2].sx = (float)ceil(e2.x);
		v[2].sy = (float)ceil(e2.y);
		v[2].sw = 0.0f;
		v[2].u = 0.5f;
		v[2].v = 0.5f;
		v[2].flags = PF_PROJECTED;
		v[2].codes = 0;
		v[2].r = gr_screen.current_color.red;
		v[2].g = gr_screen.current_color.green;
		v[2].b = gr_screen.current_color.blue;

		v[3].sx = (float)ceil(e1.x);
		v[3].sy = (float)ceil(e1.y);
		v[3].sw = 0.0f;
		v[3].u = 0.5f;
		v[3].v = 0.5f;
		v[3].flags = PF_PROJECTED;
		v[3].codes = 0;				
		v[3].r = gr_screen.current_color.red;
		v[3].g = gr_screen.current_color.green;
		v[3].b = gr_screen.current_color.blue;		

		// draw the polys
		g3_draw_poly_constant_sw(4, verts, TMAP_FLAG_GOURAUD | TMAP_FLAG_RGB, 0.1f);		

		// if we're past the first section, draw a "patch" triangle to fill any gaps
		if(idx > 0){
			// stuff coords		
			v[0].sx = (float)ceil(s1.x);
			v[0].sy = (float)ceil(s1.y);	
			v[0].sw = 0.0f;
			v[0].u = 0.5f;
			v[0].v = 0.5f;
			v[0].flags = PF_PROJECTED;
			v[0].codes = 0;
			v[0].r = gr_screen.current_color.red;
			v[0].g = gr_screen.current_color.green;
			v[0].b = gr_screen.current_color.blue;

			v[1].sx = (float)ceil(s2.x);
			v[1].sy = (float)ceil(s2.y);	
			v[1].sw = 0.0f;
			v[1].u = 0.5f;
			v[1].v = 0.5f;
			v[1].flags = PF_PROJECTED;
			v[1].codes = 0;
			v[1].r = gr_screen.current_color.red;
			v[1].g = gr_screen.current_color.green;
			v[1].b = gr_screen.current_color.blue;


			v[2].sx = (float)ceil(last_e2.x);
			v[2].sy = (float)ceil(last_e2.y);
			v[2].sw = 0.0f;
			v[2].u = 0.5f;
			v[2].v = 0.5f;
			v[2].flags = PF_PROJECTED;
			v[2].codes = 0;
			v[2].r = gr_screen.current_color.red;
			v[2].g = gr_screen.current_color.green;
			v[2].b = gr_screen.current_color.blue;

			g3_draw_poly_constant_sw(3, verts, TMAP_FLAG_GOURAUD | TMAP_FLAG_RGB, 0.1f);		
		}

		// store our endpoints
		last_e1 = e1;
		last_e2 = e2;
	}

	if(started_frame){
		g3_end_frame();
	}

	// restore zbuffer mode
	gr_zbuffer_set(saved_zbuffer_mode);

	// restore culling
	gr_set_cull(1);		
}
