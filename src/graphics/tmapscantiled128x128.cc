/*
 * Copyright (C) Volition, Inc. 1999.  All rights reserved.
 *
 * All source code herein is the property of Volition, Inc. You may not sell
 * or otherwise commercially exploit the source or things you created based on the
 * source.
 *
*/


#include <render/3d.hh>
#include <graphics/2d.hh>
#include <graphics/grinternal.hh>
#include <graphics/tmapper.hh>
#include <graphics/tmapscanline.hh>
#include <math/floating.hh>
#include <palman/palman.hh>
#include <math/fix.hh>

// Needed to keep warning 4725 to stay away.  See PsTypes.h for details why.
void disable_warning_4725_stub_tst128()
{
}

// 128x128 tiles: 7 integer bits per coordinate.  The asm that lived
// here was the tile-size specialization of the generic C mapper in
// tmapscanline.cpp.  Unlike the 16/32/64 sizes, this file's asm did
// not recompute the per-scanline setup (fx_l, fl_*_wide, fx_w); it
// used whatever the outer loop or a previous call left in Tmap, hence
// do_setup = 0.

void tmapscan_pln8_zbuffered_tiled_128x128()
{
	tmapscan_pln8_zbuffered_tiled_g( 7, 0 );
}

void tmapscan_pln8_tiled_128x128()
{
	tmapscan_pln8_tiled_g( 7, 0 );
}
