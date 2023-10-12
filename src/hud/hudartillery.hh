// -*- mode: c++; -*-

#ifndef FREESPACE2_HUD_HUDARTILLERY_HH
#define FREESPACE2_HUD_HUDARTILLERY_HH

#include <defs.hh>

// -----------------------------------------------------------------------------------------------------------------------
// ARTILLERY DEFINES/VARS
//

// SSM shapes
#define SSM_SHAPE_POINT 0
#define SSM_SHAPE_CIRCLE 1
#define SSM_SHAPE_SPHERE 2

// global ssm types
struct ssm_info  {
    char name[NAME_LENGTH]; // strike name
    int count;              // # of missiles in this type of strike
    int max_count; // Maximum # of missiles in this type of strike (-1 for no
                   // variance)
    int weapon_info_index; // missile type
    float warp_radius;     // radius of associated warp effect
    float warp_time;       // how long the warp effect lasts
    float radius;          // radius around the target ship
    float max_radius; // Maximum radius around the target ship (-1.0f for no
                      // variance)
    float offset;     // offset in front of the target ship
    float max_offset; // Maximum offset in front of the target ship (-1.0f for
                      // no variance)
    char message[NAME_LENGTH];
    bool use_custom_message;
    bool send_message;
    gamesnd_id sound_index;
    int shape;
};

// creation info for the strike (useful for multiplayer)
struct ssm_firing_info  {
    std::vector< int > delay_stamp; // timestamps
    std::vector< vec3d > start_pos; // start positions

    int count; // The ssm_info count may be variable; this stores the actual
               // number of projectiles for this strike.
    size_t ssm_index;     // index info ssm_info array
    class object* target; // target for the strike
    int ssm_team;         // team that fired the ssm.
    float duration;       // how far into the warp effect to fire
};

// the strike itself
struct ssm_strike  {
    std::vector< int > fireballs; // warpin effect fireballs
    std::vector< bool >
        done_flags; // when we've fired off the individual missiles

    // this is the info that controls how the strike behaves (just like for
    // beam weapons)
    ssm_firing_info sinfo;
};

extern std::vector< ssm_info > Ssm_info;

// -----------------------------------------------------------------------------------------------------------------------
// ARTILLERY FUNCTIONS
//

// level init
void hud_init_artillery ();

// update all hud artillery related stuff
void hud_artillery_update ();

// render all hud artillery related stuff
void hud_artillery_render ();

// start a subspace missile effect
void ssm_create (
    object* target, vec3d* start, size_t ssm_index, ssm_firing_info* override,
    int team);

// Goober5000
extern int ssm_info_lookup (const char* name);

#endif // FREESPACE2_HUD_HUDARTILLERY_HH
