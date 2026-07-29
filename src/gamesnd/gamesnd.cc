/*
 * Copyright (C) Volition, Inc. 1999.  All rights reserved.
 *
 * All source code herein is the property of Volition, Inc. You may not sell 
 * or otherwise commercially exploit the source or things you created based on the 
 * source.
 *
*/

#include <globalincs/pstypes.hh>
#include <gamesnd/gamesnd.hh>
#include <sound/sound.hh>
#include <parse/parselo.hh>
#include <localization/localize.hh>

// Global array that holds data about the gameplay sound effects.
game_snd Snds[MAX_GAME_SOUNDS];

// Global array that holds data about the interface sound effects.
game_snd Snds_iface[MAX_INTERFACE_SOUNDS];
int Snds_iface_handle[MAX_INTERFACE_SOUNDS];

// flyby sounds - 2 for each species (fighter and bomber flybys)
game_snd Snds_flyby[MAX_SPECIES_NAMES][2];

void
gamesnd_play_iface(int n)
{
    if (Snds_iface_handle[n] >= 0)
        snd_stop(Snds_iface_handle[n]);

    Snds_iface_handle[n] = snd_play(&Snds_iface[n]);
}

// load in sounds that we expect will get played
//
// The method currently used is to load all those sounds that have the hardware flag
// set.  This works well since we don't want to try and load hardware sounds in on the
// fly (too slow).
void
gamesnd_preload_common_sounds()
{
    int i;
    game_snd *gs;

    for (i = 0; i < MAX_GAME_SOUNDS; i++) {
        gs = &Snds[i];
        if (gs->filename[0] != 0 && stricmp(gs->filename, NOX("none.wav"))) {
            if (gs->preload) {
                gs->id = snd_load(gs);
            }
        }
        game_busy(); // Animate loading cursor... does nothing if loading screen not active.
    }
}

// -------------------------------------------------------------------------------------------------
// gamesnd_load_gameplay_sounds()
//
// Load the ingame sounds into memory
//
void
gamesnd_load_gameplay_sounds()
{
    int i;
    game_snd *gs;

    for (i = 0; i < MAX_GAME_SOUNDS; i++) {
        gs = &Snds[i];
        if (gs->filename[0] != 0 && stricmp(gs->filename, NOX("none.wav"))) {
            gs->id = snd_load(gs);
        }
    }
}

// -------------------------------------------------------------------------------------------------
// gamesnd_unload_gameplay_sounds()
//
// Unload the ingame sounds from memory
//
void
gamesnd_unload_gameplay_sounds()
{
    int i;
    game_snd *gs;

    for (i = 0; i < MAX_GAME_SOUNDS; i++) {
        gs = &Snds[i];
        if (gs->id != -1) {
            snd_unload(gs->id);
            gs->id = -1;
        }
    }
}

// -------------------------------------------------------------------------------------------------
// gamesnd_load_interface_sounds()
//
// Load the interface sounds into memory
//
void
gamesnd_load_interface_sounds()
{
    int i;
    game_snd *gs;

    for (i = 0; i < MAX_INTERFACE_SOUNDS; i++) {
        gs = &Snds_iface[i];
        if (gs->filename[0] != 0 && stricmp(gs->filename, NOX("none.wav"))) {
            gs->id = snd_load(gs);
        }
    }
}

// -------------------------------------------------------------------------------------------------
// gamesnd_unload_interface_sounds()
//
// Unload the interface sounds from memory
//
void
gamesnd_unload_interface_sounds()
{
    int i;
    game_snd *gs;

    for (i = 0; i < MAX_INTERFACE_SOUNDS; i++) {
        gs = &Snds_iface[i];
        if (gs->id != -1) {
            snd_unload(gs->id);
            gs->id = -1;
            gs->id_sig = -1;
        }
    }
}

// -------------------------------------------------------------------------------------------------
// gamesnd_parse_line()
//
// Parse a sound effect line
//
void
gamesnd_parse_line(game_snd *gs, const char *tag)
{
    int is_3d;

    required_string(tag);
    stuff_int(&gs->sig);
    stuff_string(gs->filename, F_NAME, ",");
    if (!stricmp(gs->filename, NOX("empty"))) {
        gs->filename[0] = 0;
        advance_to_eoln(NULL);
        return;
    }
    Mp++;
    stuff_int(&gs->preload);
    stuff_float(&gs->default_volume);
    stuff_int(&is_3d);
    if (is_3d) {
        gs->flags |= GAME_SND_USE_DS3D;
        stuff_int(&gs->min);
        stuff_int(&gs->max);
    }
    advance_to_eoln(NULL);
}

// -------------------------------------------------------------------------------------------------
// gamesnd_parse_soundstbl() will parse the sounds.tbl file, and load the specified sounds.
//
//
void
gamesnd_parse_soundstbl()
{
    int rval;
    int num_game_sounds = 0;
    int num_iface_sounds = 0;

    // open localization
    lcl_ext_open();

    gamesnd_init_sounds();

    if ((rval = setjmp(parse_abort)) != 0) {
        Error(LOCATION, "Unable to parse sounds.tbl!  Code = %i.\n", rval);
    }
    else {
        read_file_text("sounds.tbl");
        reset_parse();
    }

    // Parse the gameplay sounds section
    required_string("#Game Sounds Start");
    while (required_string_either("#Game Sounds End", "$Name:")) {
        Assert(num_game_sounds < MAX_GAME_SOUNDS);
        gamesnd_parse_line(&Snds[num_game_sounds], "$Name:");
        num_game_sounds++;
    }
    required_string("#Game Sounds End");

    // Parse the interface sounds section
    required_string("#Interface Sounds Start");
    while (required_string_either("#Interface Sounds End", "$Name:")) {
        Assert(num_iface_sounds < MAX_INTERFACE_SOUNDS);
        gamesnd_parse_line(&Snds_iface[num_iface_sounds], "$Name:");
        num_iface_sounds++;
    }
    required_string("#Interface Sounds End");

    // parse flyby sound section
    required_string("#Flyby Sounds Start");

    // read 2 terran sounds
    gamesnd_parse_line(&Snds_flyby[SPECIES_TERRAN][0], "$Terran:");
    gamesnd_parse_line(&Snds_flyby[SPECIES_TERRAN][1], "$Terran:");

    // 2 vasudan sounds
    gamesnd_parse_line(&Snds_flyby[SPECIES_VASUDAN][0], "$Vasudan:");
    gamesnd_parse_line(&Snds_flyby[SPECIES_VASUDAN][1], "$Vasudan:");

    gamesnd_parse_line(&Snds_flyby[SPECIES_SHIVAN][0], "$Shivan:");
    gamesnd_parse_line(&Snds_flyby[SPECIES_SHIVAN][1], "$Shivan:");

    required_string("#Flyby Sounds End");

    // close localization
    lcl_ext_close();
}

// -------------------------------------------------------------------------------------------------
// gamesnd_init_struct()
//
void
gamesnd_init_struct(game_snd *gs)
{
    gs->filename[0] = 0;
    gs->id = -1;
    gs->id_sig = -1;
    //   gs->is_3d = 0;
    //   gs->use_ds3d = 0;
    gs->flags = 0;
}

// -------------------------------------------------------------------------------------------------
// gamesnd_init_sounds() will initialize the Snds[] and Snds_iface[] arrays
//
void
gamesnd_init_sounds()
{
    int i;

    // init the gameplay sounds
    for (i = 0; i < MAX_GAME_SOUNDS; i++) {
        gamesnd_init_struct(&Snds[i]);
    }

    // init the interface sounds
    for (i = 0; i < MAX_INTERFACE_SOUNDS; i++) {
        gamesnd_init_struct(&Snds_iface[i]);
        Snds_iface_handle[i] = -1;
    }
}

// callback function for the UI code to call when the mouse first goes over a button.
void
common_play_highlight_sound()
{
    gamesnd_play_iface(SND_USER_OVER);
}

void
gamesnd_play_error_beep()
{
    gamesnd_play_iface(SND_GENERAL_FAIL);
}
