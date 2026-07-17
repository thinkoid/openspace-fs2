/*
 * Copyright (C) Volition, Inc. 1999.  All rights reserved.
 *
 * All source code herein is the property of Volition, Inc. You may not sell
 * or otherwise commercially exploit the source or things you created based on the
 * source.
 *
*/


#include "3d.h"
#include "2d.h"
#include "grinternal.h"
#include "tmapper.h"
#include "tmapscanline.h"
#include "floating.h"
#include "palman.h"
#include "fix.h"

// Needed to keep warning 4725 to stay away.  See PsTypes.h for details why.
void disable_warning_4725_stub_tst32()
{
}

// 32x32 tiles: 5 integer bits per coordinate.  The asm that lived here
// was the tile-size specialization of the generic C mapper in
// tmapscanline.cpp; this size recomputed the per-scanline setup itself.

void tmapscan_pln8_zbuffered_tiled_32x32()
{
	tmapscan_pln8_zbuffered_tiled_g( 5, 1 );
}

void tmapscan_pln8_tiled_32x32()
{
	tmapscan_pln8_tiled_g( 5, 1 );
}
