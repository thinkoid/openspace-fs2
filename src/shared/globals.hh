// -*- mode: c++; -*-

#ifndef FREESPACE2_SHARED_GLOBALS_HH
#define FREESPACE2_SHARED_GLOBALS_HH

#include <defs.hh>

#include <shared/types.hh>

#include <cmath>
#include <cstdint>

////////////////////////////////////////////////////////////////////////

extern int Fred_running;
extern bool running_unittests;

extern float Cutscene_bars_progress, Cutscene_delta_time;
extern int Cutscene_bar_flags;

struct shader;
extern shader Viewer_shader;

enum FadeType { FI_NONE, FI_FADEIN, FI_FADEOUT };

extern FadeType Fade_type;
extern int Fade_start_timestamp;
extern int Fade_end_timestamp;

struct vei  {
    angles_t angles; // Angles defining viewer location.
    float distance;  // Distance from which to view, plus 2x radius.
};

struct vci  {
    angles_t angles;
    float distance; // Distance from which to view, plus 3x radius
};

extern fix Missiontime;
extern fix Skybox_timestamp;
extern fix Frametime;
extern int Framecount;
extern int Game_mode;
extern int Viewer_mode;
extern int Rand_count;
extern int Game_restoring; // If set, this means we are restoring data from disk

// The detail level.  Anything below zero draws simple models earlier than it
// should.   Anything above zero draws higher detail models longer than it
// should. -2=lowest -1=low 0=normal (medium) 1=high 2=extra high
extern int Game_detail_level;

extern uint Game_detail_flags;

extern angles_t Viewer_slew_angles;
extern vei Viewer_external_info;
extern vci Viewer_chase_info;
extern vec3d leaning_position;

extern int Is_standalone;
extern int Interface_framerate; // show interface framerate during flips
extern int Interface_last_tick; // last timer tick on flip

// Noise numbers go from 0 to 1.0
extern float Noise[NOISE_NUM_FRAMES];

// override states to skip rendering of certain elements, but without disabling
// them completely
extern bool Basemap_override;
extern bool Envmap_override;
extern bool Specmap_override;
extern bool Normalmap_override;
extern bool Heightmap_override;
extern bool Glowpoint_override;
extern bool Glowpoint_use_depth_buffer;
extern bool GLSL_override;
extern bool PostProcessing_override;
extern bool Shadow_override;
extern bool Trail_render_override;

extern bool Basemap_color_override_set;
extern float Basemap_color_override[4];

extern bool Glowmap_color_override_set;
extern float Glowmap_color_override[3];

extern bool Specmap_color_override_set;
extern float Specmap_color_override[3];

extern bool Gloss_override_set;
extern float Gloss_override;

// game skill levels
#define NUM_SKILL_LEVELS 5

//====================================================================================
// DETAIL LEVEL STUFF
// If you change any of this, be sure to increment the player file version
// in FreeSpace\ManagePilot.cpp and change Detail_defaults in SystemVars.cpp
// or bad things will happen, I promise.
//====================================================================================

#define MAX_DETAIL_LEVEL 4 // The highest valid value for the "analog" detail level settings

// If you change this, update player file in ManagePilot.cpp
struct detail_levels  {
    int setting; // Which default setting this was created from.   0=lowest...
                 // NUM_DEFAULT_DETAIL_LEVELS-1, -1=Custom

    // "Analogs"
    int nebula_detail;     // 0=lowest detail, MAX_DETAIL_LEVEL=highest detail
    int detail_distance;   // 0=lowest MAX_DETAIL_LEVEL=highest
    int hardware_textures; // 0=max culling, MAX_DETAIL_LEVEL=no culling
    int num_small_debris;  // 0=min number, MAX_DETAIL_LEVEL=max number
    int num_particles;     // 0=min number, MAX_DETAIL_LEVEL=max number
    int num_stars;         // 0=min number, MAX_DETAIL_LEVEL=max number
    int shield_effects;    // 0=min, MAX_DETAIL_LEVEL=max
    int lighting;          // 0=min, MAX_DETAIL_LEVEL=max

    // Booleans
    int targetview_model; // 0=off, 1=on
    int planets_suns;     // 0=off, 1=on
    int weapon_extras;    // extra weapon details. trails, glows
};

// Global values used to access detail levels in game and libs
extern detail_levels Detail;

#define NUM_DEFAULT_DETAIL_LEVELS 4 // How many "predefined" detail levels there are

// Call this with 0=lowest, NUM_DEFAULT_DETAIL_LEVELS=highest
void detail_level_set (int level);
int current_detail_level ();

void insertion_sort (void*, size_t, size_t, int (*)(const void*, const void*));

#endif // FREESPACE2_SHARED_GLOBALS_HH
