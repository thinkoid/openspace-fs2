/*
 * Copyright (C) Volition, Inc. 1999.  All rights reserved.
 *
 * All source code herein is the property of Volition, Inc. You may not sell 
 * or otherwise commercially exploit the source or things you created based on the 
 * source.
 *
*/

#ifndef _STARFIELD_H
#define _STARFIELD_H

#include <parse/parselo.hh>
#include <cfile/cfile.hh>

#define MAX_STARFIELD_BITMAP_LISTS 1
#define MAX_STARFIELD_BITMAPS 60
#define MAX_ASTEROID_FIELDS 4

// nice low polygon background
#define BACKGROUND_MODEL_FILENAME "spherec.pof"

// global info (not individual instances)
typedef struct starfield_bitmap
{
    char filename[MAX_FILENAME_LEN + 1]; // bitmap filename
    char glow_filename[MAX_FILENAME_LEN + 1]; // only for suns
    int bitmap; // bitmap handle
    int glow_bitmap; // only for suns
    int xparent;
    float r, g, b, i; // only for suns
} starfield_bitmap;

// starfield bitmap instance
typedef struct starfield_bitmap_instance
{
    char filename[MAX_FILENAME_LEN +
                  1]; // used to match up into the starfield_bitmap array
    float scale_x, scale_y; // x and y scale
    int div_x, div_y; // # of x and y divisions
    angles ang; // angles from fred
} starfield_bitmap_instance;

// background bitmaps
extern starfield_bitmap Starfield_bitmaps[MAX_STARFIELD_BITMAPS];
extern starfield_bitmap_instance Starfield_bitmap_instance[MAX_STARFIELD_BITMAPS];
extern int Num_starfield_bitmaps;

// sun bitmaps and sun glow bitmaps
extern starfield_bitmap Sun_bitmaps[MAX_STARFIELD_BITMAPS];
extern starfield_bitmap_instance Suns[MAX_STARFIELD_BITMAPS];
extern int Num_suns;

extern const int MAX_STARS;
extern int Num_stars;

// call on game startup
void stars_init();

// call this in game_post_level_init() so we know whether we're running in full nebula mode or not
void stars_level_init();

// This *must* be called to initialize the lighting.
// You can turn off all the stars and suns and nebulas, though.
void stars_draw(int show_stars, int show_suns, int show_nebulas,
                int show_subspace);
// void calculate_bitmap_matrix(starfield_bitmaps *bm, vector *v);
// void calculate_bitmap_points(starfield_bitmaps *bm, float bank = 0.0f);

// draw the corresponding glow for sun_n
void stars_draw_sun_glow(int sun_n);

// Call when the viewer camera "cuts" so stars and debris
// don't draw incorrect blurs between last frame and this frame.
void stars_camera_cut();

// call this to set a specific model as the background model
void stars_set_background_model(char *model_name, char *texture_name);

// lookup a starfield bitmap, return index or -1 on fail
int stars_find_bitmap(char *name);

// lookup a sun by bitmap filename, return index or -1 on fail
int stars_find_sun(char *name);

// get the world coords of the sun pos on the unit sphere.
void stars_get_sun_pos(int sun_n, vector *pos);

#endif
