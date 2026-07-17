// GENERATED trap stubs for game symbols the foundation references but
// whose subsystems are not ported yet.  Calling one aborts with its name;
// data symbols read as zeros.  Entries disappear as subsystems port.
// Regenerate: link without this file, feed the demangled undefined-symbol
// list to gen_gamestubs.py (see notes.txt).

#include <stdio.h>
#include <stdlib.h>

#include "pstypes.h"

struct object;
struct ship;

static void oracle_trap(const char *sym)
{
	fprintf(stderr, "oracle strayed into unported code: %s\n", sym);
	abort();
}

void d3d_flush()
{
	oracle_trap("d3d_flush");
}

void d3d_zbias(int)
{
	oracle_trap("d3d_zbias");
}

void debug_console(void (*)())
{
}

long demo_close()
{
	return -1;
}

long demo_do_frame_end()
{
	return -1;
}

long demo_do_frame_start()
{
	return -1;
}

long demo_POST_builtin_message(int, ship*, int, int)
{
	return -1;
}

long demo_POST_departed(int, int)
{
	return -1;
}

long demo_POST_obj_create(char*, int)
{
	return -1;
}

long demo_POST_primary_fired(object*, int, int)
{
	return -1;
}

long demo_POST_ship_kill(object*)
{
	return -1;
}

long demo_POST_unique_message(char*, char*, int, int)
{
	return -1;
}

long demo_POST_warpin(int, int)
{
	return -1;
}

long demo_POST_warpout(int, int)
{
	return -1;
}

long demo_should_sim(object*)
{
	return -1;
}

long demo_start_playback(char*)
{
	return -1;
}

long demo_start_record(char*)
{
	return -1;
}

long dscap_close()
{
	return -1;
}

void gr_d3d_activate(int)
{
	oracle_trap("gr_d3d_activate");
}

void gr_d3d_bitmap_ex(int, int, int, int, int, int)
{
	oracle_trap("gr_d3d_bitmap_ex");
}

void gr_d3d_bitmap(int, int)
{
	oracle_trap("gr_d3d_bitmap");
}

void gr_d3d_cleanup()
{
	oracle_trap("gr_d3d_cleanup");
}

void gr_d3d_init()
{
	oracle_trap("gr_d3d_init");
}

void gr_d3d_preload_init()
{
	oracle_trap("gr_d3d_preload_init");
}

void gr_d3d_preload(int, int)
{
	oracle_trap("gr_d3d_preload");
}

void gr_dd_activate(int)
{
	oracle_trap("gr_dd_activate");
}

void gr_directdraw_cleanup()
{
	oracle_trap("gr_directdraw_cleanup");
}

void gr_directdraw_force_windowed()
{
	oracle_trap("gr_directdraw_force_windowed");
}

void gr_directdraw_init()
{
	oracle_trap("gr_directdraw_init");
}

void gr_glide_activate(int)
{
	oracle_trap("gr_glide_activate");
}

void gr_glide_bitmap_ex(int, int, int, int, int, int)
{
	oracle_trap("gr_glide_bitmap_ex");
}

void gr_glide_bitmap(int, int)
{
	oracle_trap("gr_glide_bitmap");
}

void gr_glide_cleanup()
{
	oracle_trap("gr_glide_cleanup");
}

void gr_glide_force_windowed()
{
	oracle_trap("gr_glide_force_windowed");
}

void gr_glide_init()
{
	oracle_trap("gr_glide_init");
}

void gr_glide_string_hack(int, int, char*)
{
	oracle_trap("gr_glide_string_hack");
}

void gr_opengl_bitmap_ex(int, int, int, int, int, int)
{
	oracle_trap("gr_opengl_bitmap_ex");
}

void gr_opengl_bitmap(int, int)
{
	oracle_trap("gr_opengl_bitmap");
}

void gr_opengl_cleanup()
{
	oracle_trap("gr_opengl_cleanup");
}

void gr_opengl_init()
{
	oracle_trap("gr_opengl_init");
}

void windebug_memwatch_init()
{
}

// data symbols, zero-backed
unsigned char D3D_32bit[1 << 20];
unsigned char D3D_fog_mode[1 << 20];
unsigned char D3D_inited[1 << 20];
unsigned char D3d_rendition_uvs[1 << 20];
unsigned char D3D_textures_in[1 << 20];
unsigned char D3D_textures_in_frame[1 << 20];
unsigned char D3D_zbias[1 << 20];
unsigned char Demo_error[1 << 20];
unsigned char Demo_make[1 << 20];
unsigned char Glide_explosion_vram[1 << 20];
unsigned char Glide_textures_in[1 << 20];
unsigned char Glide_textures_in_frame[1 << 20];
unsigned char Glide_voodoo3[1 << 20];
unsigned char TotalRam[1 << 20];
