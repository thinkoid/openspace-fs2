/*
 * Copyright (C) Volition, Inc. 1999.  All rights reserved.
 *
 * All source code herein is the property of Volition, Inc. You may not sell 
 * or otherwise commercially exploit the source or things you created based on the 
 * source.
 *
*/

#include <missionui/missiondebrief.hh>
#include <mission/missionbriefcommon.hh>
#include <missionui/missionscreencommon.hh>
#include <mission/missiongoals.hh>
#include <missionui/missionpause.hh>
#include <freespace2/freespace.hh>
#include <gamesequence/gamesequence.hh>
#include <io/key.hh>
#include <graphics/2d.hh>
#include <ui/ui.hh>
#include <ui/uidefs.hh>
#include <gamesnd/gamesnd.hh>
#include <parse/sexp.hh>
#include <parse/parselo.hh>
#include <sound/audiostr.hh>
#include <io/timer.hh>
#include <bmpman/bmpman.hh>
#include <gamehelp/contexthelp.hh>
#include <stats/stats.hh>
#include <playerman/player.hh>
#include <io/mouse.hh>
#include <gamesnd/eventmusic.hh>
#include <graphics/font.hh>
#include <popup/popup.hh>
#include <stats/medals.hh>
#include <gamehelp/contexthelp.hh>
#include <globalincs/alphacolors.hh>
#include <localization/localize.hh>
#include <osapi/osapi.hh>

#define MAX_TOTAL_DEBRIEF_LINES 200

#define TEXT_TYPE_NORMAL 1
#define TEXT_TYPE_RECOMMENDATION 2

#define DEBRIEF_NUM_STATS_PAGES 4
#define DEBRIEF_MISSION_STATS 0
#define DEBRIEF_MISSION_KILLS 1
#define DEBRIEF_ALLTIME_STATS 2
#define DEBRIEF_ALLTIME_KILLS 3

// 3rd coord is max width in pixels
int Debrief_title_coords[GR_NUM_RESOLUTIONS][3] = { { // GR_640
                                                      18, 118, 174 },
                                                    { // GR_1024
                                                      28, 193, 280 } };

int Debrief_text_wnd_coords[GR_NUM_RESOLUTIONS][4] = { { // GR_640
                                                         43, 140, 339, 303 },
                                                       { // GR_1024
                                                         69, 224, 535, 485 } };

int Debrief_text_x2[GR_NUM_RESOLUTIONS] = {
    276, // GR_640
    450 // GR_1024
};

int Debrief_stage_info_coords[GR_NUM_RESOLUTIONS][2] = { { // GR_640
                                                           379, 137 },
                                                         { // GR_1024
                                                           578, 224 } };

int Debrief_more_coords[GR_NUM_RESOLUTIONS][2] = { { // GR_640
                                                     323, 453 },
                                                   { // GR_1024
                                                     323, 453 } };

int Debrief_award_wnd_coords[GR_NUM_RESOLUTIONS][2] = { { // GR_640
                                                          411, 126 },
                                                        { // GR_1024
                                                          658, 203 } };

int Debrief_award_coords[GR_NUM_RESOLUTIONS][2] = { { // GR_640
                                                      416, 140 },
                                                    { // GR_1024
                                                      666, 224 } };

// 0=x, 1=y, 2=width of the field
int Debrief_medal_text_coords[GR_NUM_RESOLUTIONS][3] = { { // GR_640
                                                           423, 247, 189 },
                                                         { // GR_1024
                                                           666, 333, 67 } };

// 0=x, 1=y, 2=height of the field
int Debrief_award_text_coords[GR_NUM_RESOLUTIONS][3] = { { // GR_640
                                                           416, 210, 42 },
                                                         { // GR_1024
                                                           666, 333, 67 } };

// 0 = with medal
// 1 = without medal (text will use medal space)
#define DB_WITH_MEDAL 0
#define DB_WITHOUT_MEDAL 1
int Debrief_award_text_width[GR_NUM_RESOLUTIONS][2] = { { // GR_640
                                                          123, 203 },
                                                        { // GR_1024
                                                          196, 312 } };

const char *Debrief_single_name[GR_NUM_RESOLUTIONS] = {
    "DebriefSingle", // GR_640
    "2_DebriefSingle" // GR_1024
};
const char *Debrief_mask_name[GR_NUM_RESOLUTIONS] = {
    "Debrief-m", // GR_640
    "2_Debrief-m" // GR_1024
};

#define NUM_BUTTONS 18
#define NUM_TABS 2

#define DEBRIEF_TAB 0
#define STATS_TAB 1
#define TEXT_SCROLL_UP 2
#define TEXT_SCROLL_DOWN 3
#define REPLAY_MISSION 4
#define RECOMMENDATIONS 5
#define FIRST_STAGE 6
#define PREV_STAGE 7
#define NEXT_STAGE 8
#define LAST_STAGE 9
#define MULTI_PINFO_POPUP 10
#define MULTI_KICK 11
#define MEDALS_BUTTON 12
#define PLAYER_SCROLL_UP 13
#define PLAYER_SCROLL_DOWN 14
#define HELP_BUTTON 15
#define OPTIONS_BUTTON 16
#define ACCEPT_BUTTON 17

#define REPEAT 1

typedef struct
{
    char text[NAME_LENGTH + 1]; // name of ship type with a colon
    int num; // how many ships of this type player has killed
} debrief_stats_kill_info;

static ui_button_info Buttons[GR_NUM_RESOLUTIONS][NUM_BUTTONS] = {
    {
        // GR_640
        ui_button_info("DB_00", 6, 1, 37, 7, 0), // debriefing
        ui_button_info("DB_01", 6, 21, 37, 23, 1), // statistics
        ui_button_info("DB_02", 1, 195, -1, -1, 2), // scroll stats up
        ui_button_info("DB_03", 1, 236, -1, -1, 3), // scroll stats down
        ui_button_info("DB_04", 1, 428, 49, 447, 4), // replay mission
        ui_button_info("DB_05", 17, 459, 49, 464, 5), // recommendations
        ui_button_info("DB_06", 323, 454, -1, -1, 6), // first page
        ui_button_info("DB_07", 348, 454, -1, -1, 7), // prev page
        ui_button_info("DB_08", 372, 454, -1, -1, 8), // next page
        ui_button_info("DB_09", 396, 454, -1, -1, 9), // last page
        ui_button_info("DB_10", 441, 384, 433, 413, 10), // pilot info
        ui_button_info("DB_11", 511, 384, 510, 413, 11), // kick
        ui_button_info("DB_12", 613, 226, -1, -1, 12), // medals
        ui_button_info("DB_13", 615, 329, -1, -1, 13), // scroll pilots up
        ui_button_info("DB_14", 615, 371, -1, -1, 14), // scroll pilots down
        ui_button_info("DB_15", 538, 431, 500, 440, 15), // help
        ui_button_info("DB_16", 538, 455, 479, 464, 16), // options
        ui_button_info("DB_17", 573, 432, 572, 413, 17), // accept
    },
    {
        // GR_1024
        ui_button_info("2_DB_00", 10, 1, 59, 12, 0), // debriefing
        ui_button_info("2_DB_01", 10, 33, 59, 37, 1), // statistics
        ui_button_info("2_DB_02", 1, 312, -1, -1, 2), // scroll stats up
        ui_button_info("2_DB_03", 1, 378, -1, -1, 3), // scroll stats down
        ui_button_info("2_DB_04", 1, 685, 79, 715, 4), // replay mission
        ui_button_info("2_DB_05", 28, 735, 79, 743, 5), // recommendations
        ui_button_info("2_DB_06", 517, 726, -1, -1, 6), // first page
        ui_button_info("2_DB_07", 556, 726, -1, -1, 7), // prev page
        ui_button_info("2_DB_08", 595, 726, -1, -1, 8), // next page
        ui_button_info("2_DB_09", 633, 726, -1, -1, 9), // last page
        ui_button_info("2_DB_10", 706, 615, 700, 661, 10), // pilot info
        ui_button_info("2_DB_11", 817, 615, 816, 661, 11), // kick
        ui_button_info("2_DB_12", 981, 362, -1, -1, 12), // medals
        ui_button_info("2_DB_13", 984, 526, -1, -1, 13), // scroll pilots up
        ui_button_info("2_DB_14", 984, 594, -1, -1, 14), // scroll pilots down
        ui_button_info("2_DB_15", 861, 689, 801, 705, 15), // help
        ui_button_info("2_DB_16", 861, 728, 777, 744, 16), // options
        ui_button_info("2_DB_17", 917, 692, 917, 692, 17), // accept
    }
};

// text
#define NUM_DEBRIEF_TEXT 7
UI_XSTR Debrief_strings[GR_NUM_RESOLUTIONS][NUM_DEBRIEF_TEXT] = {
    {
        // GR_640
        { "Debriefing", 804, 37, 7, UI_XSTR_COLOR_GREEN, -1,
          &Buttons[0][DEBRIEF_TAB].button },
        { "Statistics", 1333, 37, 26, UI_XSTR_COLOR_GREEN, -1,
          &Buttons[0][STATS_TAB].button },
        { "Replay Mission", 444, 49, 447, UI_XSTR_COLOR_PINK, -1,
          &Buttons[0][REPLAY_MISSION].button },
        { "Recommendations", 1334, 49, 464, UI_XSTR_COLOR_GREEN, -1,
          &Buttons[0][RECOMMENDATIONS].button },
        { "Help", 928, 500, 440, UI_XSTR_COLOR_GREEN, -1,
          &Buttons[0][HELP_BUTTON].button },
        { "Options", 1036, 479, 464, UI_XSTR_COLOR_GREEN, -1,
          &Buttons[0][OPTIONS_BUTTON].button },
        { "Accept", 1035, 572, 413, UI_XSTR_COLOR_PINK, -1,
          &Buttons[0][ACCEPT_BUTTON].button },
    },
    {
        // GR_1024
        { "Debriefing", 804, 59, 12, UI_XSTR_COLOR_GREEN, -1,
          &Buttons[1][DEBRIEF_TAB].button },
        { "Statistics", 1333, 59, 47, UI_XSTR_COLOR_GREEN, -1,
          &Buttons[1][STATS_TAB].button },
        { "Replay Mission", 444, 79, 715, UI_XSTR_COLOR_PINK, -1,
          &Buttons[1][REPLAY_MISSION].button },
        { "Recommendations", 1334, 79, 743, UI_XSTR_COLOR_GREEN, -1,
          &Buttons[1][RECOMMENDATIONS].button },
        { "Help", 928, 801, 705, UI_XSTR_COLOR_GREEN, -1,
          &Buttons[1][HELP_BUTTON].button },
        { "Options", 1036, 780, 744, UI_XSTR_COLOR_GREEN, -1,
          &Buttons[1][OPTIONS_BUTTON].button },
        { "Accept", 1035, 917, 672, UI_XSTR_COLOR_PINK, -1,
          &Buttons[1][ACCEPT_BUTTON].button },
    }
};

char Debrief_current_callsign[CALLSIGN_LEN + 10];
player *Debrief_player;

static UI_WINDOW Debrief_ui_window;
static int Background_bitmap; // bitmap for the background of the debriefing
static int Award_bg_bitmap;
static int Rank_bitmap;
static int Medal_bitmap;
static int Badge_bitmap;
static int Wings_bitmap;
static int Crest_bitmap;
//static int Rank_text_bitmap;
//static int Medal_text_bitmap;
//static int Badge_text_bitmap;
static int Promoted;
static int Debrief_accepted;
static int Turned_traitor;
static int Must_replay_mission;

static int Current_mode;
static int New_mode;
static int Recommend_active;
static int Award_active;
static int Text_offset;
static int Num_text_lines = 0;
static int Num_debrief_lines = 0;
static int Text_type[MAX_TOTAL_DEBRIEF_LINES];
static char *Text[MAX_TOTAL_DEBRIEF_LINES];

static int Debrief_inited = 0;
static int New_stage;
static int Current_stage;
static int Num_stages;
static int Num_debrief_stages;
static int Stage_voice;

// static int Debrief_voice_ask_for_cd;

// voice id's for debriefing text
static int Debrief_voices[MAX_DEBRIEF_STAGES];

// time to delay voice playback when a new stage starts
#define DEBRIEF_VOICE_DELAY 400
static int Debrief_cue_voice; // timestamp to cue the playback of the voice
static int Debrief_first_voice_flag =
    1; // used to delay the first voice playback extra long

// pointer used for getting to debriefing information
debriefing Traitor_debriefing; // used when player is a traitor

// pointers to the active stages for this debriefing
static debrief_stage *Debrief_stages[MAX_DEBRIEF_STAGES];
static debrief_stage Promotion_stage, Badge_stage;
static debrief_stats_kill_info Debrief_stats_kills[MAX_SHIP_TYPES];

// already shown skip mission popup?
static int Debrief_skip_popup_already_shown = 0;

void debrief_text_init();
void debrief_accept(int ok_to_post_start_game_event = 1);

// promotion voice selection stuff
#define NUM_VOLITION_CAMPAIGNS 1
struct
{
    char campaign_name[32];
    int num_missions;
} Volition_campaigns[NUM_VOLITION_CAMPAIGNS] = { {
    BUILTIN_CAMPAIGN, // the only campaign for now, but this leaves room for a mission pack
    35 // make sure this is equal to the  number of missions you gave in the corresponding Debrief_promotion_voice_mapping
} };

// data for which voice goes w/ which mission
typedef struct voice_map
{
    char mission_file[32];
    int persona_index;
} voice_map;

voice_map Debrief_promotion_voice_mapping
    [NUM_VOLITION_CAMPAIGNS][MAX_CAMPAIGN_MISSIONS] = {
        { // FreeSpace2 campaign
          { "SM1-01.fs2", 1 },  { "SM1-02.fs2", 1 },  { "SM1-03.fs2", 1 },
          { "SM1-04.fs2", 2 },  { "SM1-05.fs2", 2 },  { "SM1-06.fs2", 2 },
          { "SM1-07.fs2", 2 },  { "SM1-08.fs2", 3 },  { "SM1-09.fs2", 3 },
          { "SM1-10.fs2", 3 },

          { "SM2-01.fs2", 6 },  { "SM2-02.fs2", 6 },  { "SM2-03.fs2", 6 },
          { "SM2-04.fs2", 7 },  { "SM2-05.fs2", 7 },  { "SM2-06.fs2", 7 },
          { "SM2-07.fs2", 8 },  { "SM2-08.fs2", 8 },  { "SM2-09.fs2", 8 },
          { "SM2-10.fs2", 8 },

          { "SM3-01.fs2", 8 },  { "SM3-02.fs2", 8 },  { "SM3-03.fs2", 8 },
          { "SM3-04.fs2", 8 },  { "SM3-05.fs2", 8 },  { "SM3-06.fs2", 9 },
          { "SM3-07.fs2", 9 },  { "SM3-08.fs2", 9 },  { "SM3-09.fs2", 9 },
          { "SM3-10.fs2", 9 }, // no debriefing for 3-10

          { "loop1-1.fs2", 4 }, { "loop1-2.fs2", 4 }, { "loop1-3.fs2", 5 },
          { "loop2-1.fs2", 4 }, { "loop2-2.fs2", 4 } }
    };

#define DB_AWARD_WINGS 0
#define DB_AWARD_MEDAL 1
#define DB_AWARD_SOC 2
#define DB_AWARD_RANK 3
#define DB_AWARD_BADGE 4
#define DB_AWARD_BG 5
static const char *Debrief_award_filename[GR_NUM_RESOLUTIONS][6] = {
    { "DebriefWings", "DebriefMedal", "DebriefCrest", "DebriefRank",
      "DebriefBadge", "DebriefAward" },
    { "2_DebriefWings", "2_DebriefMedal", "2_DebriefCrest", "2_DebriefRank",
      "2_DebriefBadge", "2_DebriefAward" }
};

#define AWARD_TEXT_MAX_LINES 5
#define AWARD_TEXT_MAX_LINE_LENGTH 128
char Debrief_award_text[AWARD_TEXT_MAX_LINES][AWARD_TEXT_MAX_LINE_LENGTH];
int Debrief_award_text_num_lines = 0;

// prototypes, you know you love 'em
void debrief_add_award_text(char *str);
void debrief_award_text_clear();

// functions
char *
debrief_tooltip_handler(char *str)
{
    if (!stricmp(str, NOX("@.Medal"))) {
        if (Award_active) {
            return XSTR("Medal", 435);
        }
    }
    else if (!stricmp(str, NOX("@.Rank"))) {
        if (Award_active) {
            return XSTR("Rank", 436);
        }
    }
    else if (!stricmp(str, NOX("@.Badge"))) {
        if (Award_active) {
            return XSTR("Badge", 437);
        }
    }
    else if (!stricmp(str, NOX("@Medal"))) {
        if (Medal_bitmap >= 0) {
            return Medals[Player->stats.m_medal_earned].name;
        }
    }
    else if (!stricmp(str, NOX("@Rank"))) {
        if (Rank_bitmap >= 0) {
            return Ranks[Promoted].name;
        }
    }
    else if (!stricmp(str, NOX("@Badge"))) {
        if (Badge_bitmap >= 0) {
            return Medals[Badge_index[Player->stats.m_badge_earned]].name;
        }
    }

    return NULL;
}

// initialize the array of handles to the different voice streams
void
debrief_voice_init()
{
    int i;

    for (i = 0; i < MAX_DEBRIEF_STAGES; i++) {
        Debrief_voices[i] = -1;
    }
}

void
debrief_load_voice_file(int voice_num, char *name)
{
    int load_attempts = 0;
    while (1) {
        if (load_attempts++ > 5) {
            break;
        }

        Debrief_voices[voice_num] = audiostream_open(name, ASF_VOICE);
        if (Debrief_voices[voice_num] >= 0) {
            break;
        }

        // couldn't load voice, ask user to insert CD (if necessary)

        // if ( Debrief_voice_ask_for_cd ) {
        // if ( game_do_cd_check() == 0 ) {
        // Debrief_voice_ask_for_cd = 0;
        // break;
        // }
        // }
    }
}

// open and pre-load the stream buffers for the different voice streams
void
debrief_voice_load_all()
{
    int i;

    // Debrief_voice_ask_for_cd = 1;

    for (i = 0; i < Num_debrief_stages; i++) {
        if (strlen(Debrief_stages[i]->voice) <= 0) {
            continue;
        }
        if (strnicmp(Debrief_stages[i]->voice, NOX("none"), 4)) {
            debrief_load_voice_file(i, Debrief_stages[i]->voice);
            //       Debrief_voices[i] = audiostream_open(Debrief_stages[i]->voice, ASF_VOICE);
        }
    }
}

// close all the briefing voice streams
void
debrief_voice_unload_all()
{
    int i;

    for (i = 0; i < MAX_DEBRIEF_STAGES; i++) {
        if (Debrief_voices[i] != -1) {
            audiostream_close_file(Debrief_voices[i], 0);
            Debrief_voices[i] = -1;
        }
    }
}

// start playback of the voice for a particular briefing stage
void
debrief_voice_play()
{
    if (!Briefing_voice_enabled || (Current_mode != DEBRIEF_TAB)) {
        return;
    }

    // no more stages?  We are done then.
    if (Stage_voice >= Num_debrief_stages) {
        return;
    }

    // if in delayed start, see if delay has elapsed and start voice if so
    if (Debrief_cue_voice) {
        if (!timestamp_elapsed(Debrief_cue_voice)) {
            return;
        }

        Stage_voice++; // move up to next voice
        if ((Stage_voice < Num_debrief_stages) &&
            (Debrief_voices[Stage_voice] >= 0)) {
            audiostream_play(Debrief_voices[Stage_voice], Master_voice_volume, 0);
            Debrief_cue_voice = 0; // indicate no longer in delayed start checking
        }

        return;
    }

    // see if voice is still playing.  If so, do nothing yet.
    if ((Stage_voice >= 0) &&
        audiostream_is_playing(Debrief_voices[Stage_voice])) {
        return;
    }

    // set voice to play in a little while from now.
    Debrief_cue_voice = timestamp(DEBRIEF_VOICE_DELAY);
}

// stop playback of the voice for a particular briefing stage
void
debrief_voice_stop()
{
    if ((Stage_voice < 0) || (Stage_voice > Num_debrief_stages) ||
        (Debrief_voices[Stage_voice] < 0))
        return;

    audiostream_stop(
        Debrief_voices[Stage_voice]); // stream is automatically rewound
    Stage_voice = -1;
}

// --------------------------------------------------------------------------------------
// debrief_set_stages()
//
// Set up the active stages for this debriefing
//
// returns:    number of active debriefing stages
//
int
debrief_set_stages()
{
    int i;
    debriefing *debriefp;

    // check to see if player is a traitor (looking at his team).  If so, use the special
    // traitor debriefing.
    debriefp = Debriefing;
    if (Player_ship->team == TEAM_TRAITOR)
        debriefp = &Traitor_debriefing;

    Num_debrief_stages = 0;
    if (Promoted >= 0) {
        Debrief_stages[Num_debrief_stages++] = &Promotion_stage;
    }

    if (Badge_bitmap >= 0) {
        Debrief_stages[Num_debrief_stages++] = &Badge_stage;
    }

    for (i = 0; i < debriefp->num_stages; i++) {
        if (eval_sexp(debriefp->stages[i].formula) == 1) {
            Debrief_stages[Num_debrief_stages++] = &debriefp->stages[i];
        }
    }

    return Num_debrief_stages;
}

// init the buttons that are specific to the debriefing screen
void
debrief_buttons_init()
{
    ui_button_info *b;
    int i;

    for (i = 0; i < NUM_BUTTONS; i++) {
        b = &Buttons[gr_screen.res][i];
        b->button.create(&Debrief_ui_window, "", b->x, b->y, 60, 30,
                         0 /*b->flags & REPEAT*/, 1);
        // set up callback for when a mouse first goes over a button
        b->button.set_highlight_action(common_play_highlight_sound);
        b->button.set_bmaps(b->filename);
        b->button.link_hotspot(b->hotspot);
    }

    // add all xstrs
    for (i = 0; i < NUM_DEBRIEF_TEXT; i++) {
        Debrief_ui_window.add_XSTR(&Debrief_strings[gr_screen.res][i]);
    }

    // set up hotkeys for buttons so we draw the correct animation frame when a key is pressed
    Buttons[gr_screen.res][NEXT_STAGE].button.set_hotkey(KEY_RIGHT);
    Buttons[gr_screen.res][PREV_STAGE].button.set_hotkey(KEY_LEFT);
    Buttons[gr_screen.res][LAST_STAGE].button.set_hotkey(KEY_SHIFTED | KEY_RIGHT);
    Buttons[gr_screen.res][FIRST_STAGE].button.set_hotkey(KEY_SHIFTED | KEY_LEFT);
    Buttons[gr_screen.res][TEXT_SCROLL_UP].button.set_hotkey(KEY_UP);
    Buttons[gr_screen.res][TEXT_SCROLL_DOWN].button.set_hotkey(KEY_DOWN);
    Buttons[gr_screen.res][ACCEPT_BUTTON].button.set_hotkey(KEY_CTRLED +
                                                            KEY_ENTER);
}

// --------------------------------------------------------------------------------------
// debrief_ui_init()
//
void
debrief_ui_init()
{
    // init ship selection masks and buttons
    common_set_interface_palette("DebriefPalette"); // set the interface palette
    Debrief_ui_window.create(0, 0, gr_screen.max_w, gr_screen.max_h, 0);
    Debrief_ui_window.set_mask_bmap(Debrief_mask_name[gr_screen.res]);
    Debrief_ui_window.tooltip_handler = debrief_tooltip_handler;
    debrief_buttons_init();

    // load in help overlay bitmap
    help_overlay_load(DEBRIEFING_OVERLAY);
    help_overlay_set_state(DEBRIEFING_OVERLAY, 0);

    Background_bitmap = bm_load(Debrief_single_name[gr_screen.res]);

    if (Background_bitmap < 0) {
        Warning(LOCATION,
                "Could not load the background bitmap for debrief screen");
    }

    Award_bg_bitmap = bm_load(Debrief_award_filename[gr_screen.res][DB_AWARD_BG]);
}

// sets Promotion_stage.voice
// defaults to number 9 (Petrarch) for non-volition missions
// this is an ugly, nasty way of doing this, but it saves us changing the missions at this point
void
debrief_choose_promotion_voice()
{
    int i, j;

    if (Campaign.current_mission < 0) {
        sprintf(Promotion_stage.voice, NOX("9_%.29s"),
                Ranks[Promoted].promotion_voice_base);
        return;
    }

    // search thru all official campaigns for our current campaign
    if ((Campaign.missions[Campaign.current_mission].name) &&
        (Campaign.filename[0])) {
        for (i = 0; i < NUM_VOLITION_CAMPAIGNS; i++) {
            if ((Campaign.filename[0]) &&
                !stricmp(Campaign.filename,
                         Volition_campaigns[i].campaign_name)) {
                // now search thru the mission filenames,
                for (j = 0; j < Volition_campaigns[i].num_missions; j++) {
                    if ((Campaign.missions[Campaign.current_mission].name !=
                         NULL) &&
                        !stricmp(
                            Campaign.missions[Campaign.current_mission].name,
                            Debrief_promotion_voice_mapping[i][j].mission_file)) {
                        // found it!  set the persona and bail
                        sprintf(
                            Promotion_stage.voice, NOX("%d_%.27s"),
                            Debrief_promotion_voice_mapping[i][j].persona_index,
                            Ranks[Promoted].promotion_voice_base);
                        return;
                    }
                }
            }
        }
    }

    // default to petrarch
    sprintf(Promotion_stage.voice, NOX("9_%.29s"),
            Ranks[Promoted].promotion_voice_base);
}

// sets Promotion_stage.voice
// defaults to number 9 (Petrarch) for non-volition missions
// this is an ugly, nasty, hateful way of doing this, but it saves us changing the missions at this point
void
debrief_choose_badge_voice()
{
    int i, j;

    if (Campaign.current_mission < 0) {
        // default to petrarch
        sprintf(Badge_stage.voice, NOX("9_%.29s"),
                Badge_info[Player->stats.m_badge_earned].voice_base);
    }

    if ((Campaign.missions[Campaign.current_mission].name) &&
        (Campaign.filename[0])) {
        // search thru all official campaigns for our current campaign
        for (i = 0; i < NUM_VOLITION_CAMPAIGNS; i++) {
            if ((Campaign.filename[0]) &&
                !stricmp(Campaign.filename,
                         Volition_campaigns[i].campaign_name)) {
                // now search thru the mission filenames,
                for (j = 0; j < Campaign.num_missions; j++) {
                    if ((Campaign.missions[Campaign.current_mission].name !=
                         NULL) &&
                        !stricmp(
                            Campaign.missions[Campaign.current_mission].name,
                            Debrief_promotion_voice_mapping[i][j].mission_file)) {
                        // found it!  set the persona and bail
                        sprintf(
                            Badge_stage.voice, NOX("%d_%.27s"),
                            Debrief_promotion_voice_mapping[i][j].persona_index,
                            Badge_info[Player->stats.m_badge_earned].voice_base);
                        return;
                    }
                }
            }
        }
    }

    // default to petrarch
    sprintf(Badge_stage.voice, NOX("9_%.29s"),
            Badge_info[Player->stats.m_badge_earned].voice_base);
}

void
debrief_award_init()
{
    char buf[80];
    int i;

    Rank_bitmap = -1;
    Medal_bitmap = -1;
    Badge_bitmap = -1;
    Wings_bitmap = -1;
    Crest_bitmap = -1;
    Promoted = -1;

    // be sure there are no old award texts floating around
    debrief_award_text_clear();

    // handle medal earned
    if (Player->stats.m_medal_earned != -1) {
        if (Player->stats.m_medal_earned == 13) { // special hack for the wings..
            int ver;
            if (Player->stats.medals[13] > 1) {
                ver = 1;
            }
            else {
                ver = 0;
            }
            sprintf(buf, NOX("%s%.2d"),
                    Debrief_award_filename[gr_screen.res][DB_AWARD_WINGS], ver);
            Wings_bitmap = bm_load(buf);
        }
        else if (Player->stats.m_medal_earned ==
                 17) { // special hack for the soc crest
            Crest_bitmap = bm_load(
                Debrief_award_filename[gr_screen.res][DB_AWARD_SOC]);
        }
        else {
            sprintf(buf, NOX("%s%.2d"),
                    Debrief_award_filename[gr_screen.res][DB_AWARD_MEDAL],
                    Player->stats.m_medal_earned);
            Medal_bitmap = bm_load(buf);
        }

        debrief_add_award_text(Medals[Player->stats.m_medal_earned].name);
    }

    // handle promotions
    if (Player->stats.m_promotion_earned != -1) {
        Promoted = Player->stats.m_promotion_earned;
        sprintf(buf, NOX("%s%.2d"),
                Debrief_award_filename[gr_screen.res][DB_AWARD_RANK],
                Promoted + 1);
        Rank_bitmap = bm_load(buf);

        Promotion_stage.new_text = Ranks[Promoted].promotion_text;
        Promotion_stage.new_recommendation_text = NULL;

        // choose appropriate promotion voice for this mission
        debrief_choose_promotion_voice();

        debrief_add_award_text(Ranks[Promoted].name);
    }

    // handle badge earned
    // only grant badge if earned and allowed.  (no_promotion really means no promotion and no badges)
    if (Player->stats.m_badge_earned != -1) {
        i = Player->stats.m_badge_earned;
        sprintf(buf, NOX("%s%.2d"),
                Debrief_award_filename[gr_screen.res][DB_AWARD_BADGE], i + 1);
        Badge_bitmap = bm_load(buf);

        Badge_stage.new_text = Badge_info[i].promotion_text;
        Badge_stage.new_recommendation_text = NULL;

        // choose appropriate voice
        debrief_choose_badge_voice();

        debrief_add_award_text(Medals[Badge_index[i]].name);
    }

    if ((Rank_bitmap >= 0) || (Medal_bitmap >= 0) || (Badge_bitmap >= 0) ||
        (Wings_bitmap >= 0) || (Crest_bitmap >= 0)) {
        Award_active = 1;
    }
    else {
        Award_active = 0;
    }
}

// debrief_traitor_init() initializes local data which could be used if the player leaves the
// mission a traitor.  The same debriefing always gets played
void
debrief_traitor_init()
{
    static int inited = 0;

    if (!inited) {
        debriefing *debrief;
        debrief_stage *stagep;
        int rval;
        int stage_num;

        if ((rval = setjmp(parse_abort)) != 0) {
            Error(LOCATION, "Unable to parse traitor.tbl!  Code = %i.\n", rval);
        }
        else {
            read_file_text("traitor.tbl");
            reset_parse();
        }

        // open localization
        lcl_ext_open();

        // simplied form of the debriefing stuff.
        debrief = &Traitor_debriefing;
        required_string("#Debriefing_info");

        required_string("$Num stages:");
        stuff_int(&debrief->num_stages);
        Assert(debrief->num_stages == 1);

        stage_num = 0;
        stagep = &debrief->stages[stage_num++];
        required_string("$Formula:");
        stagep->formula = get_sexp_main();
        required_string("$multi text");
        stagep->new_text = stuff_and_malloc_string(F_MULTITEXT, NULL,
                                                   MAX_DEBRIEF_LEN);
        required_string("$Voice:");
        char traitor_voice_file[NAME_LENGTH];
        stuff_string(traitor_voice_file, F_FILESPEC, NULL);

        // DKA 9/13/99  Only 1 traitor msg for FS2
        //     if ( Player->on_bastion ) {
        //        strcpy(stagep->voice, NOX("3_"));
        //     } else {
        //        strcpy(stagep->voice, NOX("1_"));
        //     }

        strcat(stagep->voice, traitor_voice_file);

        required_string("$Recommendation text:");
        stagep->new_recommendation_text = stuff_and_malloc_string(
            F_MULTITEXT, NULL, MAX_RECOMMENDATION_LEN);
        inited = 1;

        // close localization
        lcl_ext_close();
    }

    // disable the accept button if in single player and I am a traitor
    Debrief_accepted = 0;
    Turned_traitor = Must_replay_mission = 0;
    if (Game_mode & GM_CAMPAIGN_MODE) {
        if (Player_ship->team == TEAM_TRAITOR) {
            Turned_traitor = 1;
        }

        if (Campaign.next_mission == Campaign.current_mission) {
            Must_replay_mission = 1;
        }
    }

    if (Turned_traitor || Must_replay_mission) {
        Buttons[gr_screen.res][ACCEPT_BUTTON].button.hide();

        // kill off any stats
        Player->flags &= ~PLAYER_FLAGS_PROMOTED;
        scoring_level_init(&Player->stats);
    }
}

// get optional mission popup text
void
debrief_assemble_optional_mission_popup_text(char *buffer,
                                             char *mission_loop_desc)
{
    Assert(buffer != NULL);
    // base message

    if (mission_loop_desc == NULL) {
        strcpy(buffer, XSTR("<No Mission Loop Description Available>", 1490));
        mprintf(("No mission loop description avail"));
    }
    else {
        strcpy(buffer, mission_loop_desc);
    }

    strcat(buffer, XSTR("\n\n\nDo you want to play the optional mission?", 1491));
}

// what to do when the accept button is hit
void
debrief_accept(int ok_to_post_start_game_event)
{
    extern int Weapon_energy_cheat;
    int go_loop = 0;

    Weapon_energy_cheat = 0; // the cheat otherwise persists into the next mission

    if ((/*Cheats_enabled ||*/ Turned_traitor || Must_replay_mission) &&
        (Game_mode & GM_CAMPAIGN_MODE)) {
        char *str;
        int z;

        if (Player_ship->team == TEAM_TRAITOR) {
            str = XSTR(
                "Your career is over, Traitor!  You can't accept new missions!",
                439);
        } /* else if (Cheats_enabled) {
         str = XSTR( "You are a cheater.  You cannot accept this mission!", 440);
      }*/
        else {
            str = XSTR(
                "You have failed this mission and cannot accept.  What do you you wish to do instead?",
                441);
        }

        z = popup(0, 3, XSTR("Return to &Debriefing", 442),
                  XSTR("Go to &Flight Deck", 443), XSTR("&Replay Mission", 444),
                  str);
        if (z == 2) {
            gameseq_post_event(GS_EVENT_START_BRIEFING); // cycle back to briefing
        }
        else if (z == 1) {
            gameseq_post_event(
                GS_EVENT_END_GAME); // return to main hall, tossing stats
        }

        return;
    }

    Debrief_accepted = 1;
    // save mission stats

    int play_commit_sound = 1;
    // only write the player's stats if he's accepted

    // if we are just playing a single mission, then don't do many of the things
    // that need to be done.  Nothing much should happen when just playing a single
    // mission that isn't in a campaign.
    if (Game_mode & GM_CAMPAIGN_MODE) {
        // check for possible mission loop
        // check for (1) mission loop available, (2) dont have to repeat last mission
        int cur = Campaign.current_mission;
        bool require_repeat_mission = (Campaign.current_mission ==
                                       Campaign.next_mission);
        if (Campaign.missions[cur].has_mission_loop) {
            Assert(Campaign.loop_mission != CAMPAIGN_LOOP_MISSION_UNINITIALIZED);
        }

        if ((Campaign.missions[cur].has_mission_loop &&
             (Campaign.loop_mission != -1)) &&
            !require_repeat_mission) {
            /*
         char buffer[512];
         debrief_assemble_optional_mission_popup_text(buffer, Campaign.missions[cur].mission_loop_desc);

         int choice = popup(0 , 2, POPUP_NO, POPUP_YES, buffer);
         if (choice == 1) {
            Campaign.loop_enabled = 1;
            Campaign.next_mission = Campaign.loop_mission;
         }
         */
            go_loop = 1;
        }

        // loopy loopy time
        if (go_loop) {
            if (ok_to_post_start_game_event) {
                gameseq_post_event(GS_EVENT_LOOP_BRIEF);
            }
            else {
                play_commit_sound = 0;
            }
        }
        // continue as normal
        else {
            // end the mission
            mission_campaign_mission_over();

            // check to see if we are out of the loop now
            if (Campaign.next_mission == Campaign.loop_reentry) {
                Campaign.loop_enabled = 0;
            }

            // check if campaign is over
            if (Campaign.next_mission == -1) {
                gameseq_post_event(GS_EVENT_MAIN_MENU);
            }
            else {
                if (ok_to_post_start_game_event) {
                    // CD CHECK
                    if (game_do_cd_mission_check(Game_current_mission_filename)) {
                        gameseq_post_event(GS_EVENT_START_GAME);
                    }
                    else {
                        gameseq_post_event(GS_EVENT_MAIN_MENU);
                    }
                }
                else {
                    play_commit_sound = 0;
                }
            }
        }
    }
    else {
        gameseq_post_event(GS_EVENT_MAIN_MENU);
    }

    if (play_commit_sound) {
        gamesnd_play_iface(SND_COMMIT_PRESSED);
    }

    game_flush();
}

void
debrief_next_tab()
{
    New_mode = Current_mode + 1;
    if (New_mode >= NUM_TABS)
        New_mode = 0;
}

void
debrief_prev_tab()
{
    New_mode = Current_mode - 1;
    if (New_mode < 0)
        New_mode = NUM_TABS - 1;
}

// --------------------------------------------------------------------------------------
// debrief_next_stage()
//
void
debrief_next_stage()
{
    if (Current_stage < Num_stages - 1) {
        New_stage = Current_stage + 1;
        gamesnd_play_iface(SND_BRIEF_STAGE_CHG);
    }
    else
        gamesnd_play_iface(SND_BRIEF_STAGE_CHG_FAIL);
}

// --------------------------------------------------------------------------------------
// debrief_prev_stage()
//
void
debrief_prev_stage()
{
    if (Current_stage) {
        New_stage = Current_stage - 1;
        gamesnd_play_iface(SND_BRIEF_STAGE_CHG);
    }
    else
        gamesnd_play_iface(SND_BRIEF_STAGE_CHG_FAIL);
}

// --------------------------------------------------------------------------------------
// debrief_first_stage()
void
debrief_first_stage()
{
    if (Current_stage) {
        New_stage = 0;
        gamesnd_play_iface(SND_BRIEF_STAGE_CHG);
    }
    else
        gamesnd_play_iface(SND_BRIEF_STAGE_CHG_FAIL);
}

// --------------------------------------------------------------------------------------
// debrief_last_stage()
void
debrief_last_stage()
{
    if (Current_stage != Num_stages - 1) {
        New_stage = Num_stages - 1;
        gamesnd_play_iface(SND_BRIEF_STAGE_CHG);
    }
    else
        gamesnd_play_iface(SND_BRIEF_STAGE_CHG_FAIL);
}

// draw what stage number the debriefing is on
void
debrief_render_stagenum()
{
    int w;
    char buf[64];

    if (Num_stages < 2)
        return;

    sprintf(buf, XSTR("%d of %d", 445), Current_stage + 1, Num_stages);
    gr_get_string_size(&w, NULL, buf);
    gr_set_color_fast(&Color_bright_blue);
    gr_string(Debrief_stage_info_coords[gr_screen.res][0] - w,
              Debrief_stage_info_coords[gr_screen.res][1], buf);
    gr_set_color_fast(&Color_white);
}

// render the mission time at the specified y location
void
debrief_render_mission_time(int y_loc)
{
    char time_str[30];

    game_format_time(Missiontime, time_str);
    gr_string(0, y_loc, XSTR("Mission Time", 446));
    gr_string(Debrief_text_x2[gr_screen.res], y_loc, time_str);
}

// render out the debriefing text to the scroll window
void
debrief_render()
{
    int y, z, font_height;

    if (Num_stages <= 0)
        return;

    font_height = gr_get_font_height();

    gr_set_clip(Debrief_text_wnd_coords[gr_screen.res][0],
                Debrief_text_wnd_coords[gr_screen.res][1],
                Debrief_text_wnd_coords[gr_screen.res][2],
                Debrief_text_wnd_coords[gr_screen.res][3]);
    y = 0;
    z = Text_offset;
    while (y + font_height <= Debrief_text_wnd_coords[gr_screen.res][3]) {
        if (z >= Num_text_lines)
            break;

        if (Text_type[z] == TEXT_TYPE_NORMAL)
            gr_set_color_fast(&Color_white);
        else
            gr_set_color_fast(&Color_bright_red);

        if (Text[z])
            gr_string(0, y, Text[z]);

        y += font_height;
        z++;
    }

    gr_reset_clip();
}

// render out the stats info to the scroll window
//
void
debrief_stats_render()
{
    int i, y, font_height;

    gr_set_color_fast(&Color_blue);
    gr_set_clip(Debrief_text_wnd_coords[gr_screen.res][0],
                Debrief_text_wnd_coords[gr_screen.res][1],
                Debrief_text_wnd_coords[gr_screen.res][2],
                Debrief_text_wnd_coords[gr_screen.res][3]);
    gr_string(0, 0, Debrief_current_callsign);
    font_height = gr_get_font_height();
    y = 30;

    switch (Current_stage) {
    case DEBRIEF_MISSION_STATS:
        i = Current_stage - 1;
        if (i < 0)
            i = 0;

        gr_set_color_fast(&Color_white);

        // display mission completion time
        debrief_render_mission_time(y);

        y += 20;
        show_stats_label(i, 0, y, font_height);
        show_stats_numbers(i, Debrief_text_x2[gr_screen.res], y, font_height);
        break;
    case DEBRIEF_ALLTIME_STATS:
        i = Current_stage - 1;
        if (i < 0)
            i = 0;

        gr_set_color_fast(&Color_white);
        show_stats_label(i, 0, y, font_height);
        show_stats_numbers(i, Debrief_text_x2[gr_screen.res], y, font_height);
        break;

    case DEBRIEF_ALLTIME_KILLS:
    case DEBRIEF_MISSION_KILLS:
        gr_set_color_fast(&Color_white);
        i = Text_offset;
        while (y + font_height <= Debrief_text_wnd_coords[gr_screen.res][3]) {
            if (i >= Num_text_lines)
                break;

            if (!i) {
                if (Current_stage == DEBRIEF_MISSION_KILLS)
                    gr_printf(0, y, XSTR("Mission Kills by Ship Type", 447));
                else
                    gr_printf(0, y, XSTR("All-time Kills by Ship Type", 448));
            }
            else if (i > 1) {
                gr_printf(0, y, "%s", Debrief_stats_kills[i - 2].text);
                gr_printf(Debrief_text_x2[gr_screen.res], y, "%d",
                          Debrief_stats_kills[i - 2].num);
            }

            y += font_height;
            i++;
        }

        if (Num_text_lines == 2) {
            if (Current_stage == DEBRIEF_MISSION_KILLS)
                gr_printf(0, y, XSTR("(No ship kills this mission)", 449));
            else
                gr_printf(0, y, XSTR("(No ship kills)", 450));
        }

        break;

    default:
        Int3();
        break;
    }

    gr_reset_clip();
}

// do action for when the replay button is pressed
void
debrief_replay_pressed()
{
    if (!Turned_traitor && !Must_replay_mission &&
        (Game_mode & GM_CAMPAIGN_MODE)) {
        int choice;
        choice = popup(
            0, 2, POPUP_CANCEL, XSTR("&Replay", 451),
            XSTR(
                "If you choose to replay this mission, you will be required to complete it again before proceeding to future missions.\n\nIn addition, any statistics gathered during this mission will be discarded if you choose to replay.",
                452));

        if (choice != 1) {
            return;
        }
    }

    gameseq_post_event(GS_EVENT_START_BRIEFING); // take us to the briefing
    gamesnd_play_iface(SND_COMMIT_PRESSED);
}

// -------------------------------------------------------------------
// debrief_redraw_pressed_buttons()
//
// Redraw any debriefing buttons that are pressed down.  This function is needed
// since we sometimes need to draw pressed buttons last to ensure the entire
// button gets drawn (and not overlapped by other buttons)
//
void
debrief_redraw_pressed_buttons()
{
    int i;
    UI_BUTTON *b;

    for (i = 0; i < NUM_BUTTONS; i++) {
        b = &Buttons[gr_screen.res][i].button;
        // don't draw the recommendations button if we're in stats mode
        if (b->button_down()) {
            b->draw_forced(2);
        }
    }
}

// debrief specific button with hotspot 'i' has been pressed, so perform the associated action
//
void
debrief_button_pressed(int num)
{
    switch (num) {
    case DEBRIEF_TAB:
        Buttons[gr_screen.res][RECOMMENDATIONS].button.enable();
        // Debrief_ui_window.use_hack_to_get_around_stupid_problem_flag = 0;
        if (num != Current_mode) {
            gamesnd_play_iface(SND_SCREEN_MODE_PRESSED);
        }
        New_mode = num;
        break;
    case STATS_TAB:
        // Debrief_ui_window.use_hack_to_get_around_stupid_problem_flag = 1;        // allows failure sound to be played
        Buttons[gr_screen.res][RECOMMENDATIONS].button.disable();
        if (num != Current_mode) {
            gamesnd_play_iface(SND_SCREEN_MODE_PRESSED);
        }
        New_mode = num;
        break;

    case TEXT_SCROLL_UP:
        if (Text_offset) {
            Text_offset--;
            gamesnd_play_iface(SND_SCROLL);
        }
        else {
            gamesnd_play_iface(SND_GENERAL_FAIL);
        }
        break;

    case TEXT_SCROLL_DOWN:
        if (Text_offset +
                Debrief_text_wnd_coords[gr_screen.res][3] / gr_get_font_height() <
            Num_text_lines) {
            Text_offset++;
            gamesnd_play_iface(SND_SCROLL);
        }
        else {
            gamesnd_play_iface(SND_GENERAL_FAIL);
        }
        break;

    case REPLAY_MISSION:
        debrief_replay_pressed();
        break;

    case RECOMMENDATIONS:
        gamesnd_play_iface(SND_USER_SELECT);
        Recommend_active = !Recommend_active;
        debrief_text_init();
        break;

    case FIRST_STAGE:
        debrief_first_stage();
        break;

    case PREV_STAGE:
        debrief_prev_stage();
        break;

    case NEXT_STAGE:
        debrief_next_stage();
        break;

    case LAST_STAGE:
        debrief_last_stage();
        break;

    case HELP_BUTTON:
        gamesnd_play_iface(SND_HELP_PRESSED);
        launch_context_help();
        break;

    case OPTIONS_BUTTON:
        gamesnd_play_iface(SND_SWITCH_SCREENS);
        gameseq_post_event(GS_EVENT_OPTIONS_MENU);
        break;

    case ACCEPT_BUTTON:
        debrief_accept();
        break;

    case MEDALS_BUTTON:
        gamesnd_play_iface(SND_SWITCH_SCREENS);
        gameseq_post_event(GS_EVENT_VIEW_MEDALS);
        break;
    } // end swtich
}

void
debrief_setup_ship_kill_stats(int stage_num)
{
    int i;
    ushort *kill_arr;
    debrief_stats_kill_info *kill_info;

    Assert(Current_stage < DEBRIEF_NUM_STATS_PAGES);
    if (Current_stage == DEBRIEF_MISSION_STATS ||
        Current_stage == DEBRIEF_ALLTIME_STATS)
        return;

    Assert(Debrief_player != NULL);

    // kill_ar points to an array of MAX_SHIP_TYPE ints
    if (Current_stage == DEBRIEF_MISSION_KILLS) {
        kill_arr = Debrief_player->stats.m_okKills;
    }
    else {
        kill_arr = Debrief_player->stats.kills;
    }

    Num_text_lines = 0;
    for (i = 0; i < MAX_SHIP_TYPES; i++) {
        // code used to add in mission kills, but the new system assumes that the player will accept, so
        // all time stats already have mission stats added in.
        if (kill_arr[i] <= 0) {
            continue;
        }

        kill_info = &Debrief_stats_kills[Num_text_lines++];

        kill_info->num = kill_arr[i];

        strcpy(kill_info->text, Ship_info[i].name);
        strcat(kill_info->text, NOX(":"));
    }

    Num_text_lines += 2;
}

// Iterate through the debriefing buttons, checking if they are pressed
void
debrief_check_buttons()
{
    int i;

    for (i = 0; i < NUM_BUTTONS; i++) {
        if (Buttons[gr_screen.res][i].button.pressed()) {
            debrief_button_pressed(i);
        }
    }
}

void
debrief_text_stage_init(char *src, int type)
{
    int i, n_lines, n_chars[MAX_DEBRIEF_LINES];
    char line[MAX_DEBRIEF_LINE_LEN];
    char *p_str[MAX_DEBRIEF_LINES];

    n_lines = split_str(src, Debrief_text_wnd_coords[gr_screen.res][2], n_chars,
                        p_str, MAX_DEBRIEF_LINES);
    Assert(n_lines >= 0);

    // if you hit this, you proba
    if (n_lines >= MAX_DEBRIEF_LINES) {
        Warning(
            LOCATION,
            "You have come close to the limit of debriefing lines, try adding more stages");
    }

    for (i = 0; i < n_lines; i++) {
        Assert(n_chars[i] < MAX_DEBRIEF_LINE_LEN);
        Assert(Num_text_lines < MAX_TOTAL_DEBRIEF_LINES);
        strncpy(line, p_str[i], n_chars[i]);
        line[n_chars[i]] = 0;
        drop_white_space(line);
        Text_type[Num_text_lines] = type;
        Text[Num_text_lines++] = strdup(line);
    }

    return;
}

void
debrief_free_text()
{
    int i;

    for (i = 0; i < Num_debrief_lines; i++)
        if (Text[i])
            free(Text[i]);

    Num_debrief_lines = 0;
}

// setup the debriefing text lines for rendering
void
debrief_text_init()
{
    int i, r_count = 0;
    char *src;

    // release old text lines first
    debrief_free_text();
    Num_text_lines = Text_offset = 0;

    if (Current_mode == DEBRIEF_TAB) {
        for (i = 0; i < Num_debrief_stages; i++) {
            if (i)
                Text[Num_text_lines++] = NULL; // add a blank line between stages

            src = Debrief_stages[i]->new_text;
            if (src)
                debrief_text_stage_init(src, TEXT_TYPE_NORMAL);

            if (Recommend_active) {
                src = Debrief_stages[i]->new_recommendation_text;
                if (!src && (i == Num_debrief_stages - 1) && !r_count)
                    src = XSTR("We have no recommendations for you.", 1054);

                if (src) {
                    Text[Num_text_lines++] = NULL;
                    debrief_text_stage_init(src, TEXT_TYPE_RECOMMENDATION);
                    r_count++;
                }
            }
        }

        Num_debrief_lines = Num_text_lines;
        return;
    }

    // not in debriefing mode, must be in stats mode
    Num_text_lines = 0;
    debrief_setup_ship_kill_stats(Current_stage);
}

// --------------------------------------------------------------------------------------
//
void
debrief_init()
{
    Assert(!Debrief_inited);
    //   Campaign.loop_enabled = 0;
    Campaign.loop_mission = CAMPAIGN_LOOP_MISSION_UNINITIALIZED;

    // set up the right briefing for this guy
    Debriefing = &Debriefings[0];

    // no longer is mission
    Game_mode &= ~(GM_IN_MISSION);

    game_flush();
    Current_mode = -1;
    New_mode = DEBRIEF_TAB;
    Recommend_active = Award_active = 0;
    Current_stage = 0;

    Current_stage = -1;
    New_stage = 0;
    Debrief_cue_voice = 0;
    Num_text_lines = Num_debrief_lines = 0;
    Debrief_first_voice_flag = 1;

    if (Game_mode & GM_CAMPAIGN_MODE) {
        // MUST store goals and events first - may be used to evaluate next mission
        // store goals and events
        mission_campaign_store_goals_and_events();

        // evaluate next mission
        mission_campaign_eval_next_mission();
    }

    // call traitor init before calling scoring_level_close.  traitor init will essentially nullify
    // any stats
    debrief_traitor_init(); // initialize data needed if player becomes traitor.

    // call scoring level close for my stats.  Needed for award_init.  The stats will
    // be backed out if used chooses to replace them.
    scoring_level_close();

    debrief_ui_init(); // init UI items
    debrief_award_init();
    show_stats_init();
    debrief_voice_init();
    //   rank_bitmaps_clear();
    //   rank_bitmaps_load();

    strcpy(Debrief_current_callsign, Player->callsign);
    Debrief_player = Player;

    // set up the Debrief_stages[] and Recommendations[] arrays
    debrief_set_stages();

    if (Num_debrief_stages <= 0) {
        Num_debrief_stages = 0;
    }
    else {
        debrief_voice_load_all();
    }

    /*
   if (mission_evaluate_primary_goals() == PRIMARY_GOALS_COMPLETE) {
      common_music_init(SCORE_DEBRIEF_SUCCESS);
   } else {
      common_music_init(SCORE_DEBRIEF_FAIL);
   }
   */

    // start up the appropriate music.  outside a campaign next_mission always
    // equals current_mission, so gate the failed-the-mission test on campaign mode
    if ((Game_mode & GM_CAMPAIGN_MODE) &&
        (Campaign.next_mission == Campaign.current_mission)) {
        // you failed the mission because you suck, so you get the suck music
        common_music_init(SCORE_DEBRIEF_FAIL);
    }
    else if (mission_goals_met()) {
        // you completed all primaries and secondaries, thus you are a stud boy and you get stud boy music
        common_music_init(SCORE_DEBRIEF_SUCCESS);
    }
    else {
        // you somehow passed the mission, so you get a little something for your efforts.
        common_music_init(SCORE_DEBRIEF_AVERAGE);
    }

    if ((Game_mode & GM_CAMPAIGN_MODE) &&
        (Campaign.next_mission == Campaign.current_mission)) {
        // better luck next time, increase his retries
        Player->failures_this_session++;
    }
    else {
        // clear his retries info regardless of whether or not he accepts
        Player->failures_this_session = 0;
    }

    Buttons[gr_screen.res][PLAYER_SCROLL_UP].button.disable();
    Buttons[gr_screen.res][PLAYER_SCROLL_DOWN].button.disable();
    Buttons[gr_screen.res][MULTI_PINFO_POPUP].button.disable();
    Buttons[gr_screen.res][MULTI_KICK].button.disable();
    Buttons[gr_screen.res][PLAYER_SCROLL_UP].button.hide();
    Buttons[gr_screen.res][PLAYER_SCROLL_DOWN].button.hide();
    Buttons[gr_screen.res][MULTI_PINFO_POPUP].button.hide();
    Buttons[gr_screen.res][MULTI_KICK].button.hide();

    if (!Award_active) {
        Buttons[gr_screen.res][MEDALS_BUTTON].button.disable();
        Buttons[gr_screen.res][MEDALS_BUTTON].button.hide();
    }

    Debrief_skip_popup_already_shown = 0;

    Debrief_inited = 1;
}

// --------------------------------------------------------------------------------------
// debrief_close()
void
debrief_close()
{
    int i;

    Assert(Debrief_inited);

    // if the mission wasn't accepted, clear out my stats
    if (!Debrief_accepted || !(Game_mode & GM_CAMPAIGN_MODE)) {
        scoring_backout_accept(&Player->stats);
    }

    // if dude passed the misson and accepted, reset his show skip popup flag
    if (Debrief_accepted) {
        Player->show_skip_popup = 1;
    }

    if (Num_debrief_lines) {
        for (i = 0; i < Num_debrief_lines; i++) {
            if (Text[i]) {
                free(Text[i]);
            }
        }
    }

    // unload the overlay bitmap
    //   help_overlay_unload(DEBRIEFING_OVERLAY);

    // clear out award text
    Debrief_award_text_num_lines = 0;

    debrief_voice_unload_all();
    common_music_close();

    //   rank_bitmaps_release();

    // unload bitmaps
    if (Background_bitmap >= 0) {
        bm_unload(Background_bitmap);
    }

    if (Award_bg_bitmap >= 0) {
        bm_unload(Award_bg_bitmap);
    }

    if (Rank_bitmap >= 0) {
        bm_unload(Rank_bitmap);
    }

    if (Medal_bitmap >= 0) {
        bm_unload(Medal_bitmap);
    }

    if (Badge_bitmap >= 0) {
        bm_unload(Badge_bitmap);
    }

    if (Wings_bitmap >= 0) {
        bm_unload(Wings_bitmap);
    }

    if (Crest_bitmap >= 0) {
        bm_unload(Crest_bitmap);
    }

    Debrief_ui_window.destroy();
    common_free_interface_palette(); // restore game palette
    show_stats_close();

    game_flush();

    Debrief_inited = 0;
}

// handle keypresses in debriefing
void
debrief_do_keys(int new_k)
{
    switch (new_k) {
    case KEY_TAB:
        debrief_next_tab();
        break;

    case KEY_SHIFTED | KEY_TAB:
        debrief_prev_tab();
        break;

    case KEY_ESC: {
        int pf_flags;
        int choice;

        // display the normal debrief popup
        if (!Turned_traitor && !Must_replay_mission &&
            (Game_mode & GM_CAMPAIGN_MODE)) {
            pf_flags =
                PF_BODY_BIG; // | PF_USE_AFFIRMATIVE_ICON | PF_USE_NEGATIVE_ICON;
            choice = popup(pf_flags, 3, POPUP_CANCEL, XSTR("&Yes", 454),
                           XSTR("&No, retry later", 455),
                           XSTR("Accept this mission outcome?", 456));
            if (choice == 1) { // accept and continue on
                debrief_accept(0);
                gameseq_post_event(GS_EVENT_MAIN_MENU);
            }

            if (choice < 1)
                break;
        }
        else if (Must_replay_mission && (Game_mode & GM_CAMPAIGN_MODE)) {
            // need to popup saying that mission was a failure and must be replayed
            choice = popup(
                0, 2, POPUP_NO, POPUP_YES,
                XSTR(
                    "Because this mission was a failure, you must replay this mission when you continue your campaign.\n\nReturn to the Flight Deck?",
                    457));
            if (choice <= 0)
                break;
        }

        // Return to Main Hall
        gameseq_post_event(GS_EVENT_END_GAME);
    }

    default:
        break;
    } // end switch
}

// uuuuuugly
void
debrief_draw_award_text()
{
    int start_y, curr_y, i, x, sw;
    int fh = gr_get_font_height();
    int field_width =
        (Medal_bitmap > 0)
            ? Debrief_award_text_width[gr_screen.res][DB_WITH_MEDAL]
            : Debrief_award_text_width[gr_screen.res][DB_WITHOUT_MEDAL];

    // vertically centered within field
    start_y = Debrief_award_text_coords[gr_screen.res][1] +
              ((Debrief_award_text_coords[gr_screen.res][2] -
                (fh * Debrief_award_text_num_lines)) /
               2);
    curr_y = start_y;

    // draw the strings
    for (i = 0; i < Debrief_award_text_num_lines; i++) {
        gr_get_string_size(&sw, NULL, Debrief_award_text[i]);
        x = (Medal_bitmap < 0) ? (Debrief_award_text_coords[gr_screen.res][0] +
                                  (field_width - sw) / 2)
                               : Debrief_award_text_coords[gr_screen.res][0];
        if (i == AWARD_TEXT_MAX_LINES - 1)
            x += 7; // hack because of the shape of the box
        gr_set_color_fast(&Color_white);
        gr_string(x, curr_y, Debrief_award_text[i]);

        // adjust y pos, including a little extra between the "pairs"
        curr_y += fh;
        if ((i == 1) || (i == 3)) {
            curr_y += ((gr_screen.res == GR_640) ? 2 : 6);
        }
    }
}

// clears out text array so we dont have old award text showing up on new awards.
void
debrief_award_text_clear()
{
    int i;

    Debrief_award_text_num_lines = 0;
    for (i = 0; i < AWARD_TEXT_MAX_LINES; i++) {
        //Debrief_award_text[i][0] = 0;
        memset(Debrief_award_text[i], 0,
               sizeof(char) * AWARD_TEXT_MAX_LINE_LENGTH);
    }
}

// this is the nastiest code I have ever written.  if you are modifying this, i feel bad for you.
void
debrief_add_award_text(char *str)
{
    Assert(Debrief_award_text_num_lines <= AWARD_TEXT_MAX_LINES);
    if (Debrief_award_text_num_lines > AWARD_TEXT_MAX_LINES) {
        return;
    }

    char *line2;
    int field_width =
        (Medal_bitmap > 0)
            ? Debrief_award_text_width[gr_screen.res][DB_WITH_MEDAL]
            : Debrief_award_text_width[gr_screen.res][DB_WITHOUT_MEDAL];

    // copy in the line
    strcpy(Debrief_award_text[Debrief_award_text_num_lines], str);

    // maybe translate for displaying
    if (Lcl_gr) {
        medals_translate_name(Debrief_award_text[Debrief_award_text_num_lines],
                              AWARD_TEXT_MAX_LINE_LENGTH);
    }

    Debrief_award_text_num_lines++;

    // if its too long, split once ONLY
    // assumes text isnt > 2 lines, but this is a safe assumption due to the char limits of the ranks/badges/etc
    if (Debrief_award_text_num_lines < AWARD_TEXT_MAX_LINES) {
        line2 = split_str_once(
            Debrief_award_text[Debrief_award_text_num_lines - 1], field_width);
        if (line2 != NULL) {
            sprintf(Debrief_award_text[Debrief_award_text_num_lines], " %s",
                    line2); // indent a space
        }
        Debrief_award_text_num_lines++; // leave blank line even if it all fits into 1
    }
}

// called once per frame to drive all the input reading and rendering
void
debrief_do_frame(float frametime)
{
    int k = 0, new_k = 0;
    char buf[256];

    Assert(Debrief_inited);

    if (help_overlay_active(DEBRIEFING_OVERLAY)) {
        Buttons[gr_screen.res][HELP_BUTTON].button.reset_status();
        Debrief_ui_window.set_ignore_gadgets(1);
    }

    if (Game_mode & GM_NORMAL) {
        new_k = Debrief_ui_window.process(k);
    }
    else {
        new_k = Debrief_ui_window.process(k, 0);
    }

    if ((k > 0) || (new_k > 0) || B1_JUST_RELEASED) {
        if (help_overlay_active(DEBRIEFING_OVERLAY)) {
            help_overlay_set_state(DEBRIEFING_OVERLAY, 0);
            Debrief_ui_window.set_ignore_gadgets(0);
            k = 0;
            new_k = 0;
        }
    }

    if (!help_overlay_active(DEBRIEFING_OVERLAY)) {
        Debrief_ui_window.set_ignore_gadgets(0);
    }

    // see if the mode has changed and handle it if so.
    if (Current_mode != New_mode) {
        debrief_voice_stop();
        Current_mode = New_mode;
        Current_stage = -1;
        New_stage = 0;
        if (New_mode == DEBRIEF_TAB) {
            Num_stages = 1;
            Debrief_cue_voice = 0;
            Stage_voice = -1;
            if (Debrief_first_voice_flag) {
                Debrief_cue_voice = timestamp(DEBRIEF_VOICE_DELAY * 3);
                Debrief_first_voice_flag = 0;
            }
        }
        else {
            Num_stages = DEBRIEF_NUM_STATS_PAGES;
        }
    }

    if ((Num_stages > 0) && (New_stage != Current_stage)) {
        Current_stage = New_stage;
        debrief_text_init();
    }

    debrief_voice_play();
    common_music_do();

    // Now do all the rendering for the frame
    GR_MAYBE_CLEAR_RES(Background_bitmap);
    if (Background_bitmap >= 0) {
        gr_set_bitmap(Background_bitmap);
        gr_bitmap(0, 0);
    }

    // draw the damn awarded stuff, G
    if (Award_active && (Award_bg_bitmap >= 0)) {
        gr_set_bitmap(Award_bg_bitmap);
        gr_bitmap(Debrief_award_wnd_coords[gr_screen.res][0],
                  Debrief_award_wnd_coords[gr_screen.res][1]);
        if (Rank_bitmap >= 0) {
            gr_set_bitmap(Rank_bitmap);
            gr_bitmap(Debrief_award_coords[gr_screen.res][0],
                      Debrief_award_coords[gr_screen.res][1]);
        }

        if (Medal_bitmap >= 0) {
            gr_set_bitmap(Medal_bitmap);
            gr_bitmap(Debrief_award_coords[gr_screen.res][0],
                      Debrief_award_coords[gr_screen.res][1]);
        }

        if (Badge_bitmap >= 0) {
            gr_set_bitmap(Badge_bitmap);
            gr_bitmap(Debrief_award_coords[gr_screen.res][0],
                      Debrief_award_coords[gr_screen.res][1]);
        }

        if (Wings_bitmap >= 0) {
            gr_set_bitmap(Wings_bitmap);
            gr_bitmap(Debrief_award_coords[gr_screen.res][0],
                      Debrief_award_coords[gr_screen.res][1]);
        }

        if (Crest_bitmap >= 0) {
            gr_set_bitmap(Crest_bitmap);
            gr_bitmap(Debrief_award_coords[gr_screen.res][0],
                      Debrief_award_coords[gr_screen.res][1]);
        }

        //  draw medal/badge/rank labels
        debrief_draw_award_text();

        /*     if (Rank_text_bitmap >= 0) {
         gr_set_bitmap(Rank_text_bitmap);
         gr_bitmap(Debrief_award_coords[gr_screen.res][0], Debrief_award_coords[gr_screen.res][1]);
      }

   
      if (Medal_text_bitmap >= 0) {
         gr_set_bitmap(Medal_text_bitmap);
         gr_bitmap(Debrief_award_text_coords[gr_screen.res][0], Debrief_award_text_coords[gr_screen.res][1]);
      }

      if (Badge_text_bitmap >= 0) {
         gr_set_bitmap(Badge_text_bitmap);
         gr_bitmap(Debrief_award_text_coords[gr_screen.res][0], Debrief_award_text_coords[gr_screen.res][1]);
      }
*/
    }

    Debrief_ui_window.draw();
    debrief_redraw_pressed_buttons();
    Buttons[gr_screen.res][Current_mode].button.draw_forced(2);
    if (Recommend_active && (Current_mode != STATS_TAB)) {
        Buttons[gr_screen.res][RECOMMENDATIONS].button.draw_forced(2);
    }

    // draw the title of the mission
    gr_set_color_fast(&Color_bright_white);
    strcpy(buf, The_mission.name);
    gr_force_fit_string(buf, 255, Debrief_title_coords[gr_screen.res][2]);
    gr_string(Debrief_title_coords[gr_screen.res][0],
              Debrief_title_coords[gr_screen.res][1], buf);

#if !defined(NDEBUG) || defined(INTERPLAYQA)
    gr_set_color_fast(&Color_normal);
    gr_printf(Debrief_title_coords[gr_screen.res][0],
              Debrief_title_coords[gr_screen.res][1] - 10,
              NOX("[name: %s, mod: %s]"), Mission_filename, The_mission.modified);
#endif

    // draw the screen-specific text
    switch (Current_mode) {
    case DEBRIEF_TAB:
        if (Num_debrief_stages <= 0) {
            gr_set_color_fast(&Color_white);
            Assert(Game_current_mission_filename != NULL);
            gr_printf(Debrief_text_wnd_coords[gr_screen.res][0],
                      Debrief_text_wnd_coords[gr_screen.res][1],
                      XSTR("No Debriefing for mission: %s", 458),
                      Game_current_mission_filename);
        }
        else {
            debrief_render();
        }

        break;

    case STATS_TAB:
        debrief_stats_render();
        break;
    } // end switch

    if (Text_offset +
            Debrief_text_wnd_coords[gr_screen.res][3] / gr_get_font_height() <
        Num_text_lines) {
        int w;

        gr_set_color_fast(&Color_red);
        gr_get_string_size(&w, NULL, XSTR("More", 459));
        gr_printf(Debrief_text_wnd_coords[gr_screen.res][0] +
                      Debrief_text_wnd_coords[gr_screen.res][2] / 2 - w / 2,
                  Debrief_text_wnd_coords[gr_screen.res][1] +
                      Debrief_text_wnd_coords[gr_screen.res][3],
                  XSTR("More", 459));
    }

    debrief_render_stagenum();

    // AL 3-6-98: Needed to move key reading here, since popups are launched from this code, and we don't
    //              want to include the mouse pointer which is drawn in the flip

    if (!help_overlay_active(DEBRIEFING_OVERLAY)) {
        debrief_check_buttons();
        debrief_do_keys(new_k);
    }

    // blit help overlay if active
    help_overlay_maybe_blit(DEBRIEFING_OVERLAY);

    gr_flip();

    // dont let dude skip 3-09.  hack.
    if (Game_mode & GM_CAMPAIGN_MODE) {
        if ((Campaign.current_mission >= 0) &&
            (Campaign.current_mission < MAX_CAMPAIGN_MISSIONS)) {
            if ((Campaign.missions[Campaign.current_mission].name != NULL) &&
                !stricmp(Campaign.missions[Campaign.current_mission].name,
                         "sm3-09.fs2")) {
                Debrief_skip_popup_already_shown = 1;
            }
        }
    }

    // maybe show skip mission popup
    if ((!Debrief_skip_popup_already_shown) && (Player->show_skip_popup) &&
        (Game_mode & GM_NORMAL) && (Game_mode & GM_CAMPAIGN_MODE) &&
        (Player->failures_this_session >= PLAYER_MISSION_FAILURE_LIMIT)) {
        int popup_choice = popup(
            0, 3, XSTR("Do Not Skip This Mission", 1473),
            XSTR("Advance To The Next Mission", 1474),
            XSTR("Don't Show Me This Again", 1475),
            XSTR(
                "You have failed this mission five times.  If you like, you may advance to the next mission.",
                1472));
        switch (popup_choice) {
        case 0:
            // stay on this mission, so proceed to normal debrief
            // in other words, do nothing.
            break;
        case 1:
            // skip this mission
            mission_campaign_skip_to_next();
            gameseq_post_event(GS_EVENT_START_GAME);
            break;
        case 2:
            // dont show this again
            Player->show_skip_popup = 0;
            break;
        }

        Debrief_skip_popup_already_shown = 1;
    }
}

void
debrief_disable_accept()
{ }
