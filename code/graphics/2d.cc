/*
 * Copyright (C) Volition, Inc. 1999.  All rights reserved.
 *
 * All source code herein is the property of Volition, Inc. You may not sell 
 * or otherwise commercially exploit the source or things you created based on the 
 * source.
 *
*/ 

#if defined(__i386__) || defined(__x86_64__)
#include <cpuid.h>
#endif

#include <globalincs/pstypes.hh>
#include <osapi/osapi.hh>
#include <graphics/2d.hh>
#include <render/3d.hh>
#include <bmpman/bmpman.hh>
#include <palman/palman.hh>
#include <graphics/font.hh>
#include <graphics/grinternal.hh>
#include <globalincs/systemvars.hh>

// 3dnow stuff
// #include <amd3d.h>

// Includes for different rendering systems
#include <graphics/grsoft.hh>
#include <graphics/gropengl.hh>

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

	if ( gr_screen.mode == GR_OPENGL )	{
		gr_opengl_cleanup();
	} else {
		gr_soft_cleanup();
	}

	gr_font_close();

	Gr_inited = 0;
}

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

// -----------------------------------------------------------------------
// Returns cpu type.
void gr_detect_cpu(int *cpu, int *mmx, int *amd3d, int *katmai )
{
	uint RegEDX;
	uint RegEAX;

	// Set defaults
	*cpu = 0;
	*mmx = 0;
	*amd3d = 0;
	*katmai = 0;

	char cpu_vender[16];
	memset( cpu_vender, 0, sizeof(cpu_vender) );

#if defined(__i386__) || defined(__x86_64__)
	// was pushfd/popfd bit-21 probing plus emitted CPUID opcode bytes
	uint a, b, c, d;
	if ( __get_cpuid( 0, &a, &b, &c, &d ) )	{
		memcpy( cpu_vender+0, &b, 4 );
		memcpy( cpu_vender+4, &d, 4 );
		memcpy( cpu_vender+8, &c, 4 );

		__get_cpuid( 1, &a, &b, &c, &d );
		RegEAX = a;			// family, etc returned in eax
		RegEDX = d;			// features returned in edx
	} else {
		RegEAX = 4<<8;		// no CPUID: treat as a 486
		RegEDX = 0;
	}
#else
	RegEAX = 4<<8;
	RegEDX = 0;
#endif

	(void)cpu_vender;		// only the commented-out AMD 3Dnow check read it

	//RegEAX	.  Bits 11:8 is family
	*cpu = (RegEAX >>8) & 0xF;

	if ( *cpu < 5 )	{
		*cpu = 4;								// processor does not support CPUID
		*mmx = 0;
	}

	//RegEAX	.  Bits 11:8 is family
	*cpu = (RegEAX >>8) & 0xF;

	// Check for MMX.  Retail confirmed the CPUID bit by executing an "emms"
	// under SEH; the bit alone is trustworthy.
	if (RegEDX & 0x800000)               // bit 23 is set for MMX technology
	{
		*mmx = 1;
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
		if ( gr_screen.mode == GR_OPENGL )	{
			gr_opengl_cleanup();
		} else {
			gr_soft_cleanup();
		}
	} else {
		first_time = 1;
	}

	// Retail (HARDWARE_ONLY) forced Glide here when a non-hardware mode was
	// requested; the hardware backends are gone, software is the mode.

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
	Assert( (mode == GR_SOFTWARE) || (mode == GR_OPENGL) );
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

	// retail allowed software only for the tools (FRED/pofview);
	// here it is the default game renderer, with GL opt-in via -opengl
	if ( gr_screen.mode == GR_OPENGL )	{
		gr_opengl_init();
	} else {
		gr_soft_init();
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

	if ( gr_screen.mode == GR_OPENGL )	{
		gr_opengl_force_windowed();
	} else {
		extern void gr_soft_force_windowed();
		gr_soft_force_windowed();
	}

	if ( Os_debugger_running )
		os_sleep(1000);

}

void gr_activate(int active)
{
	if ( !Gr_inited ) return;

	if ( gr_screen.mode == GR_OPENGL )	{
		gr_opengl_activate(active);
	} else {
		extern void gr_soft_activate(int active);
		gr_soft_activate(active);
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

// new bitmap functions.  Retail dispatched these by renderer mode (they are
// not vtable entries -- gf_bitmap was commented out of the screen struct)
void gr_bitmap(int x, int y)
{
	if ( gr_screen.mode == GR_OPENGL )	{
		gr_opengl_bitmap(x, y);
	} else {
		grx_bitmap(x, y);
	}
}

void gr_bitmap_ex(int x, int y, int w, int h, int sx, int sy)
{
	if ( gr_screen.mode == GR_OPENGL )	{
		gr_opengl_bitmap_ex(x, y, w, h, sx, sy);
	} else {
		grx_bitmap_ex(x, y, w, h, sx, sy);
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
