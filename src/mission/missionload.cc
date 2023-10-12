// -*- mode: c++; -*-

#include <defs.hh>
#include <cfile/cfilesystem.hh>
#include "freespace2/freespace.hh"
#include <gamesequence/gamesequence.hh>
#include <shared/alphacolors.hh>
#include <hud/hudparse.hh>
#include <io/key.hh>
#include <mission/missioncampaign.hh>
#include <mission/missionload.hh>
#include <mission/missionparse.hh>
#include <missionui/missionshipchoice.hh>
#include <playerman/managepilot.hh>
#include <ui/ui.hh>
#include <tracing/tracing.hh>
#include <log/log.hh>

extern mission The_mission; // need to send this info to the briefing
extern int shifted_ascii_table[];
extern int ascii_table[];

std::vector< std::string > Ignored_missions;

// -----------------------------------------------
// For recording most recent missions played
// -----------------------------------------------
char Recent_missions[MAX_RECENT_MISSIONS][MAX_FILENAME_LEN];
int Num_recent_missions;

// -----------------------------------------------------
// ml_update_recent_missions()
//
// Update the Recent_missions[][] array
//
void ml_update_recent_missions (char* filename) {
    char tmp[MAX_RECENT_MISSIONS][MAX_FILENAME_LEN], *p;
    int i, j;

    for (i = 0; i < Num_recent_missions; i++) {
        strcpy (tmp[i], Recent_missions[i]);
    }

    // get a pointer to just the basename of the filename (including extension)
    p = strrchr (filename, '/');
    if (p == NULL) { p = filename; }
    else {
        p++;
    }

    ASSERT (strlen (p) < MAX_FILENAME_LEN);
    strcpy (Recent_missions[0], p);

    j = 1;
    for (i = 0; i < Num_recent_missions; i++) {
        if (strcasecmp (Recent_missions[0], tmp[i]) != 0) {
            strcpy (Recent_missions[j++], tmp[i]);
            if (j >= MAX_RECENT_MISSIONS) { break; }
        }
    }

    Num_recent_missions = j;
    ASSERT (Num_recent_missions <= MAX_RECENT_MISSIONS);
}

bool mission_is_ignored (const char* filename) {
    std::string filename_no_ext = filename;
    drop_extension (filename_no_ext);
    std::transform (
        filename_no_ext.begin (), filename_no_ext.end (),
        filename_no_ext.begin (), ::tolower);

    for (auto& ii : Ignored_missions) {
        if (ii == filename_no_ext) { return true; }
    }

    return false;
}

// Mission_load takes no parameters.
// It sets the following global variables
// Game_current_mission_filename

// returns -1 if failed, 0 if successful
int mission_load (char* filename_ext) {
    TRACE_SCOPE (tracing::LoadMissionLoad);

    char filename[MAX_PATH_LEN], *ext;

    if ((filename_ext != NULL) &&
        (Game_current_mission_filename != filename_ext))
        strncpy (
            Game_current_mission_filename, filename_ext, MAX_FILENAME_LEN - 1);

    WARNINGF (LOCATION, "MISSION LOAD: '%s'", filename_ext);

    strcpy (filename, filename_ext);
    ext = strrchr (filename, '.');
    if (ext) {
        WARNINGF (LOCATION, "Hmmm... Extension passed to mission_load...");
        *ext = 0; // remove any extension!
    }

    if (mission_is_ignored (filename)) {
        WARNINGF (LOCATION,"MISSION LOAD: Tried to load an ignored mission!  Aborting...");
        return -1;
    }

    strcat (filename, FS_MISSION_FILE_EXT);

    // does the magical mission parsing
    // creates all objects, except for the player object
    // save the player object later since the player may get
    // to choose the type of ship that he is to fly
    // return value of 0 indicates success, other is failure.

    if (parse_main (filename)) return -1;

    if (Select_default_ship) {
        int ret;
        ret = create_default_player_ship ();
        ASSERT (!ret);
    }

    ml_update_recent_missions (
        filename_ext); // update recently played missions list (save the csg
                       // later)

    init_hud ();
    return 0;
}

//====================================
// Mission Load Menu stuff
#define MLM_MAX_MISSIONS 256
int mlm_active = 0;
UI_WINDOW mlm_window;
UI_LISTBOX mlm_mission_list;
UI_LISTBOX recent_mission_list;
UI_LISTBOX campaign_filter;

UI_BUTTON mlm_ok, mlm_cancel;
char* mlm_missions[MLM_MAX_MISSIONS];
char* recent_missions[MAX_RECENT_MISSIONS];
char* campaign_names[MAX_CAMPAIGNS + 2];
char* campaign_missions[MAX_CAMPAIGN_MISSIONS];
int mlm_nfiles = 0;
static int last_recent_current = -1;
static int last_mlm_current = -1;
static int Campaign_filter_index;

char* jtmp_missions[MLM_MAX_MISSIONS];
int jtmp_nfiles = 0;

void ml_change_listbox () {
    if (!Num_recent_missions || !mlm_nfiles) return;

    if (mlm_mission_list.current () != -1) {
        mlm_mission_list.set_current (-1);
        last_mlm_current = -1;
        recent_mission_list.set_focus ();
        recent_mission_list.set_current (0);
        return;
    }

    if (recent_mission_list.current () != -1) {
        recent_mission_list.set_current (-1);
        last_recent_current = -1;
        mlm_mission_list.set_focus ();
        mlm_mission_list.set_current (0);
        return;
    }
}

static char Campaign_missions[MAX_CAMPAIGN_MISSIONS][NAME_LENGTH];
static char Campaign_name_list[MAX_CAMPAIGNS + 2][NAME_LENGTH];
static int Num_campaign_missions;

// get the mission filenames that make up a campaign
extern int mission_campaign_get_filenames (
    char* filename, char dest[][NAME_LENGTH], int* num);

void mission_load_menu_init () {
    int i;
    char wild_card[256];
    ASSERT (mlm_active == 0);
    mlm_active = 1;

    memset (wild_card, 0, 256);
    strcpy (wild_card, NOX ("*"));
    strcat (wild_card, FS_MISSION_FILE_EXT);
    mlm_nfiles = cf_get_file_list (
        MLM_MAX_MISSIONS, mlm_missions, CF_TYPE_MISSIONS, wild_card,
        CF_SORT_NAME);
    jtmp_nfiles = 0;

    ASSERT (mlm_nfiles <= MLM_MAX_MISSIONS);

    mlm_window.create (100, 100, 500, 300, 0); // WIN_DIALOG

    mlm_ok.create (&mlm_window, NOX ("Ok"), 125, 420, 80, 40);
    mlm_cancel.create (&mlm_window, NOX ("Cancel"), 250, 420, 80, 40);
    mlm_cancel.set_hotkey (KEY_ESC);

    mlm_mission_list.create (
        &mlm_window, 450, 150, 150, 200, mlm_nfiles, mlm_missions);

    for (i = 0; i < Num_recent_missions; i++) {
        recent_missions[i] = Recent_missions[i];
    }
    recent_mission_list.create (
        &mlm_window, 250, 150, 150, 200, Num_recent_missions, recent_missions);

    mlm_mission_list.set_focus ();
    mlm_mission_list.set_current (0);

    mission_campaign_build_list (0);
    for (i = 0; i < Num_campaigns; i++) {
        strcpy (Campaign_name_list[i + 1], Campaign_names[i]);
    }
    strcpy (Campaign_name_list[0], NOX ("All campaigns"));
    strcpy (Campaign_name_list[1], NOX ("Player Missions"));

    for (i = 0; i < Num_campaigns + 2; i++) {
        campaign_names[i] = Campaign_name_list[i];
    }

    campaign_filter.create (
        &mlm_window, 50, 150, 150, 200, Num_campaigns + 2, campaign_names);
    Campaign_filter_index = 0;
    campaign_filter.set_current (Campaign_filter_index);
}

void mission_load_menu_do () {
    int selected, key_in, recent_current, mlm_current, use_recent_flag, i;

    ASSERT (mlm_active == 1);

    key_in = mlm_window.process ();

    if (key_in) {
        switch (key_in & KEY_MASK) {
        case KEY_UP:
        case KEY_DOWN:
        case KEY_HOME:
        case KEY_END:
        case KEY_PAGEUP:
        case KEY_PAGEDOWN:
        case KEY_ENTER: break;

        case KEY_RIGHT:
        case KEY_LEFT: ml_change_listbox (); break;

        case KEY_ESC: gameseq_post_event (GS_EVENT_MAIN_MENU); break;

        default: break;

        } // end switch
    }

    if (campaign_filter.current () != Campaign_filter_index) {
        Campaign_filter_index = campaign_filter.current ();

        if (Campaign_filter_index > 1) {
            mission_campaign_get_filenames (
                Campaign_file_names[Campaign_filter_index - 2],
                Campaign_missions, &Num_campaign_missions);

            for (i = 0; i < Num_campaign_missions; i++) {
                campaign_missions[i] = Campaign_missions[i];
            }
            mlm_mission_list.set_new_list (
                Num_campaign_missions, campaign_missions);
        }
        else if (Campaign_filter_index == 0) {
            mlm_mission_list.set_new_list (mlm_nfiles, mlm_missions);
        }
        else if (Campaign_filter_index == 1) {
            mlm_mission_list.set_new_list (jtmp_nfiles, jtmp_missions);
        }
    }

    mlm_current = mlm_mission_list.current ();
    recent_current = recent_mission_list.current ();

    if (mlm_current != last_mlm_current) {
        recent_mission_list.set_current (-1);
        last_recent_current = -1;
    }
    last_mlm_current = mlm_current;

    if (recent_current != last_recent_current) {
        mlm_mission_list.set_current (-1);
        last_mlm_current = -1;
    }
    last_recent_current = recent_current;

    if (mlm_cancel.pressed ()) gameseq_post_event (GS_EVENT_MAIN_MENU);

    // Check if they hit OK, if so, use the current listbox
    // selection.
    selected = -1;
    use_recent_flag = 0;
    if (mlm_ok.pressed ()) {
        selected = mlm_mission_list.current ();
        if (selected == -1) {
            selected = recent_mission_list.current ();
            use_recent_flag = 1;
        }
    }
    else {
        // If they didn't hit OK, then check for a double-click on
        // a list box item.
        selected = mlm_mission_list.selected ();
        if (selected == -1) {
            selected = recent_mission_list.selected ();
            use_recent_flag = 1;
        }
    }

    char mission_name_final[512] = "";

    if (selected > -1) {
        Campaign.current_mission = -1;
        if (use_recent_flag) {
            strncpy (
                mission_name_final, recent_missions[selected],
                MAX_FILENAME_LEN);
        }
        else {
            char mission_name[NAME_LENGTH];
            if (Campaign_filter_index == 0) {
                strcpy (mission_name, mlm_missions[selected]);
            }
            else if (Campaign_filter_index == 1) {
                strcpy (mission_name, jtmp_missions[selected]);
            }
            else {
                strcpy (mission_name, Campaign_missions[selected]);
            }
            strncpy (mission_name_final, mission_name, MAX_FILENAME_LEN);
        }

        // go
        strcpy (Game_current_mission_filename, mission_name_final);
        WARNINGF (LOCATION, "Selected '%s'", Game_current_mission_filename);
        gameseq_post_event (GS_EVENT_START_GAME);
    }

    gr_clear ();
    gr_set_color_fast (&Color_bright);
    int w;
    gr_get_string_size (&w, NULL, NOX ("Select Mission"));
    gr_printf_menu (
        (gr_screen.clip_width_unscaled - w) / 2, 10, NOX ("Select Mission"));

    gr_printf_menu (50, 135, NOX ("Campaign Filter"));
    gr_printf_menu (250, 135, NOX ("Recently Played"));
    gr_printf_menu (450, 135, NOX ("Mission List"));
    mlm_window.draw ();

    gr_flip ();
}

void mission_load_menu_close () {
    int i;

    ASSERT (mlm_active == 1);
    mlm_active = 0;

    for (i = 0; i < mlm_nfiles; i++) {
        if (mlm_missions[i]) {
            free (mlm_missions[i]);
            mlm_missions[i] = NULL;
        }
    }

    for (i = 0; i < jtmp_nfiles; i++) {
        if (jtmp_missions[i]) {
            free (jtmp_missions[i]);
            jtmp_missions[i] = NULL;
        }
    }

    mlm_window.destroy ();
}
