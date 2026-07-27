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
void disable_warning_4725_stub_tst16()
{
}

// 16x16 tiles: 4 integer bits per coordinate.  The asm that lived here
// was the tile-size specialization of the generic C mapper in
// tmapscanline.cpp; this size recomputed the per-scanline setup itself.

void tmapscan_pln8_zbuffered_tiled_16x16()
{
	tmapscan_pln8_zbuffered_tiled_g( 4, 1 );
}

void tmapscan_pln8_tiled_16x16()
{
	tmapscan_pln8_tiled_g( 4, 1 );
}
