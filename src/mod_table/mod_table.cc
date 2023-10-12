// -*- mode: c++; -*-

#include <defs.hh>
#include <assert/assert.hh>
#include <log/log.hh>

/*
 * Created by Hassan "Karajorma" Kazmi for the FreeSpace2 Source Code Project.
 * You may not sell or otherwise commercially exploit the source or things you
 * create based on the source.
 */

#include <gamesnd/eventmusic.hh>
#include <shared/version.hh>
#include <localization/localize.hh>
#include <mission/missioncampaign.hh>
#include <mission/missionload.hh>
#include <mission/missionmessage.hh>
#include <missionui/fictionviewer.hh>
#include <mod_table/mod_table.hh>
#include <parse/parselo.hh>
#include <sound/sound.hh>

int Directive_wait_time;
bool True_loop_argument_sexps;
bool Fixed_turret_collisions;
bool Damage_impacted_subsystem_first;
bool Cutscene_camera_displays_hud;
bool Alternate_chaining_behavior;
int Default_ship_select_effect;
int Default_weapon_select_effect;
int Default_fiction_viewer_ui;
bool Enable_external_shaders;
bool Enable_external_default_scripts;
int Default_detail_level;
bool Full_color_head_anis;
bool Weapons_inherit_parent_collision_group;
bool Flight_controls_follow_eyepoint_orientation;
int FS2NetD_port;
float Briefing_window_FOV;
bool Disable_hc_message_ani;
bool Red_alert_applies_to_delayed_ships;
bool Beams_use_damage_factors;
float Generic_pain_flash_factor;
float Shield_pain_flash_factor;
gameversion::version Targetted_version; // Defaults to retail
std::string Window_title;
bool Unicode_text_mode;
std::string Movie_subtitle_font;
bool Enable_scripts_in_fred; // By default FRED does not initialize the
                             // scripting system
std::string Window_icon_path;
bool Disable_built_in_translations;
bool Weapon_shockwaves_respect_huge;

void parse_mod_table (const char* filename) {
    // std::vector<std::string> lines;

    try {
        read_file_text (filename, CF_TYPE_TABLES);
        reset_parse ();

        // start parsing
        optional_string ("#GAME SETTINGS");

        if (optional_string ("$Minimum version:") ||
            optional_string ("$Target Version:")) {
            Targetted_version = gameversion::parse_version ();

            WARNINGF (LOCATION, "Game Settings Table: Parsed target version of %s",gameversion::format_version (Targetted_version).c_str ());

            if (!gameversion::check_at_least (Targetted_version)) {
                ASSERTX (0, "This modification needs at least version %s of FreeSpace Open. However, the current is only %s!",gameversion::format_version (Targetted_version).c_str (),gameversion::format_version (gameversion::get_executable_version ()).c_str ());
            }
        }

        if (optional_string ("$Window title:")) {
            stuff_string (Window_title, F_NAME);
        }

        if (optional_string ("$Window icon:")) {
            stuff_string (Window_icon_path, F_NAME);
        }

        if (optional_string ("$Unicode mode:")) {
            stuff_boolean (&Unicode_text_mode);

            WARNINGF (LOCATION, "Game settings table: Unicode mode: %s",Unicode_text_mode ? "yes" : "no");
        }

        optional_string ("#CAMPAIGN SETTINGS");

        if (optional_string ("$Default Campaign File Name:")) {
            char temp[MAX_FILENAME_LEN];
            stuff_string (temp, F_NAME, MAX_FILENAME_LEN);

            // remove extension?
            if (drop_extension (temp)) {
                WARNINGF (LOCATION,"Game Settings Table: Removed extension on default campaign file name %s",temp);
            }

            // check length
            size_t maxlen = (MAX_FILENAME_LEN - 4);
            auto len = strlen (temp);
            if (len > maxlen) {
                error_display (
                    0,
                    "Token too long: [%s].  Length = %zu"
                    ".  Max is %zu.",
                    temp, len, maxlen);
                temp[maxlen] = 0;
            }

            strcpy (Default_campaign_file_name, temp);
        }

        if (optional_string ("#Ignored Campaign File Names")) {
            std::string campaign_name;

            while (optional_string ("$Campaign File Name:")) {
                stuff_string (campaign_name, F_NAME);

                // remove extension?
                if (drop_extension (campaign_name)) {
                    WARNINGF (LOCATION,"Game Settings Table: Removed extension on ignored campaign file name %s",campaign_name.c_str ());
                }

                // we want case-insensitive matching, so make this lowercase
                std::transform (
                    campaign_name.begin (), campaign_name.end (),
                    campaign_name.begin (), ::tolower);

                Ignored_campaigns.push_back (campaign_name);
            }
        }

        // Note: this feature does not ignore missions that are contained in
        // campaigns
        if (optional_string ("#Ignored Mission File Names")) {
            std::string mission_name;

            while (optional_string ("$Mission File Name:")) {
                stuff_string (mission_name, F_NAME);

                // remove extension?
                if (drop_extension (mission_name)) {
                    WARNINGF (LOCATION,"Game Settings Table: Removed extension on ignored mission file name %s",mission_name.c_str ());
                }

                // we want case-insensitive matching, so make this lowercase
                std::transform (
                    mission_name.begin (), mission_name.end (),
                    mission_name.begin (), ::tolower);

                Ignored_missions.push_back (mission_name);
            }
        }

        if (optional_string ("$Red-alert applies to delayed ships:")) {
            stuff_boolean (&Red_alert_applies_to_delayed_ships);
            if (Red_alert_applies_to_delayed_ships) {
                WARNINGF (LOCATION,"Game Settings Table: Red-alert stats will be loaded for ships that arrive later in missions");
            }
            else {
                WARNINGF (LOCATION,"Game Settings Table: Red-alert stats will NOT be loaded for ships that arrive later in missions (this is retail behavior)");
            }
        }

        optional_string ("#HUD SETTINGS");

        // how long should the game wait before displaying a directive?
        if (optional_string ("$Directive Wait Time:")) {
            stuff_int (&Directive_wait_time);
        }

        if (optional_string ("$Cutscene camera displays HUD:")) {
            stuff_boolean (&Cutscene_camera_displays_hud);
        }
        // compatibility
        if (optional_string ("$Cutscene camera disables HUD:")) {
            WARNINGF (LOCATION,"Game Settings Table: \"$$Cutscene camera disables HUD\" is deprecated in favor of \"$Cutscene camera displays HUD\"");
            bool temp;
            stuff_boolean (&temp);
            Cutscene_camera_displays_hud = !temp;
        }

        if (optional_string ("$Full color head animations:")) {
            stuff_boolean (&Full_color_head_anis);
        }
        // compatibility
        if (optional_string ("$Color head animations with hud colors:")) {
            WARNINGF (LOCATION,"Game Settings Table: \"$Color head animations with hud colors\" is deprecated in favor of \"$Full color head animations\"");
            bool temp;
            stuff_boolean (&temp);
            Full_color_head_anis = !temp;
        }

        optional_string ("#SEXP SETTINGS");

        if (optional_string ("$Loop SEXPs Then Arguments:")) {
            stuff_boolean (&True_loop_argument_sexps);
            if (True_loop_argument_sexps) {
                WARNINGF (LOCATION,"Game Settings Table: Using Reversed Loops For SEXP Arguments");
            }
            else {
                WARNINGF (LOCATION,"Game Settings Table: Using Standard Loops For SEXP Arguments");
            }
        }

        if (optional_string ("$Use Alternate Chaining Behavior:")) {
            stuff_boolean (&Alternate_chaining_behavior);
            if (Alternate_chaining_behavior) {
                WARNINGF (LOCATION,"Game Settings Table: Using alternate event chaining behavior");
            }
            else {
                WARNINGF (LOCATION,"Game Settings Table: Using standard event chaining behavior");
            }
        }

        optional_string ("#GRAPHICS SETTINGS");

        if (optional_string ("$Enable External Shaders:")) {
            stuff_boolean (&Enable_external_shaders);
            if (Enable_external_shaders)
                WARNINGF (LOCATION,"Game Settings Table: External shaders are enabled");
            else
                WARNINGF (LOCATION,"Game Settings Table: External shaders are DISABLED");
        }

        if (optional_string ("$Default Detail Level:")) {
            int detail_level;

            stuff_int (&detail_level);

            WARNINGF (LOCATION,"Game Settings Table: Setting default detail level to %i of %i-%i",detail_level, 0, NUM_DEFAULT_DETAIL_LEVELS - 1);

            if (detail_level < 0 ||
                detail_level > NUM_DEFAULT_DETAIL_LEVELS - 1) {
                error_display (
                    0, "Invalid detail level: %i, setting to %i", detail_level,
                    Default_detail_level);
            }
            else {
                Default_detail_level = detail_level;
            }
        }

        if (optional_string ("$Briefing Window FOV:")) {
            float fov;

            stuff_float (&fov);

            WARNINGF (LOCATION,"Game Settings Table: Setting briefing window FOV from %f to %f",Briefing_window_FOV, fov);

            Briefing_window_FOV = fov;
        }

        if (optional_string ("$Generic Pain Flash Factor:")) {
            stuff_float (&Generic_pain_flash_factor);
            if (Generic_pain_flash_factor != 1.0f)
                WARNINGF (LOCATION,"Game Settings Table: Setting generic pain flash factor to %.2f",Generic_pain_flash_factor);
        }

        if (optional_string ("$Shield Pain Flash Factor:")) {
            stuff_float (&Shield_pain_flash_factor);
            if (Shield_pain_flash_factor != 0.0f)
                WARNINGF (LOCATION,"Game Settings Table: Setting shield pain flash factor to %.2f",Shield_pain_flash_factor);
        }

        if (optional_string ("$BMPMAN Slot Limit:")) {
            int tmp;
            stuff_int (&tmp);

            WARNINGF (LOCATION,"Game Settings Table: $BMPMAN Slot Limit is deprecated and should be removed. It is not needed anymore.");
        }

        optional_string ("#NETWORK SETTINGS");

        if (optional_string ("$FS2NetD port:")) {
            stuff_int (&FS2NetD_port);
            if (FS2NetD_port)
                WARNINGF (LOCATION,"Game Settings Table: FS2NetD connecting to port %i",FS2NetD_port);
        }

        optional_string ("#SOUND SETTINGS");

        if (optional_string ("$Default Sound Volume:")) {
            stuff_float (&Master_sound_volume);
        }

        if (optional_string ("$Default Music Volume:")) {
            stuff_float (&Master_event_music_volume);
        }

        if (optional_string ("$Default Voice Volume:")) {
            stuff_float (&Master_voice_volume);
        }

        optional_string ("#FRED SETTINGS");

        if (optional_string ("$Disable Hard Coded Message Head Ani Files:")) {
            stuff_boolean (&Disable_hc_message_ani);
            if (Disable_hc_message_ani) {
                WARNINGF (LOCATION,"Game Settings Table: FRED - Disabling Hard Coded Message Ani Files");
            }
            else {
                WARNINGF (LOCATION,"Game Settings Table: FRED - Using Hard Coded Message Ani Files");
            }
        }

        if (optional_string ("$Enable scripting in FRED:")) {
            stuff_boolean (&Enable_scripts_in_fred);
            if (Enable_scripts_in_fred) {
                WARNINGF (LOCATION,"Game Settings Table: FRED - Scripts will be executed when running FRED.");
            }
            else {
                WARNINGF (LOCATION,"Game Settings Table: FRED - Scripts will not be executed when running FRED.");
            }
        }

        optional_string ("#OTHER SETTINGS");

        if (optional_string ("$Fixed Turret Collisions:")) {
            stuff_boolean (&Fixed_turret_collisions);
        }

        if (optional_string ("$Damage Impacted Subsystem First:")) {
            stuff_boolean (&Damage_impacted_subsystem_first);
        }

        if (optional_string ("$Default ship select effect:")) {
            char effect[NAME_LENGTH];
            stuff_string (effect, F_NAME, NAME_LENGTH);
            if (!strcasecmp (effect, "FS2"))
                Default_ship_select_effect = 2;
            else if (!strcasecmp (effect, "FS1"))
                Default_ship_select_effect = 1;
            else if (!strcasecmp (effect, "off"))
                Default_ship_select_effect = 0;
        }

        if (optional_string ("$Default weapon select effect:")) {
            char effect[NAME_LENGTH];
            stuff_string (effect, F_NAME, NAME_LENGTH);
            if (!strcasecmp (effect, "FS2"))
                Default_weapon_select_effect = 2;
            else if (!strcasecmp (effect, "FS1"))
                Default_weapon_select_effect = 1;
            else if (!strcasecmp (effect, "off"))
                Default_weapon_select_effect = 0;
        }

        if (optional_string ("$Weapons inherit parent collision group:")) {
            stuff_boolean (&Weapons_inherit_parent_collision_group);
            if (Weapons_inherit_parent_collision_group)
                WARNINGF (LOCATION,"Game Settings Table: Weapons inherit parent collision group");
        }

        if (optional_string (
                "$Flight controls follow eyepoint orientation:")) {
            stuff_boolean (&Flight_controls_follow_eyepoint_orientation);
            if (Flight_controls_follow_eyepoint_orientation)
                WARNINGF (LOCATION,"Game Settings Table: Flight controls follow eyepoint orientation");
        }

        if (optional_string ("$Beams Use Damage Factors:")) {
            stuff_boolean (&Beams_use_damage_factors);
            if (Beams_use_damage_factors) {
                WARNINGF (LOCATION,"Game Settings Table: Beams will use Damage Factors");
            }
            else {
                WARNINGF (LOCATION,"Game Settings Table: Beams will ignore Damage Factors (retail behavior)");
            }
        }

        if (optional_string ("$Default fiction viewer UI:")) {
            char ui_name[NAME_LENGTH];
            stuff_string (ui_name, F_NAME, NAME_LENGTH);
            if (!strcasecmp (ui_name, "auto"))
                Default_fiction_viewer_ui = -1;
            else {
                int ui_index = fiction_viewer_ui_name_to_index (ui_name);
                if (ui_index >= 0)
                    Default_fiction_viewer_ui = ui_index;
                else
                    error_display (
                        0, "Unrecognized fiction viewer UI: %s", ui_name);
            }
        }

        if (optional_string ("$Movie subtitle font:")) {
            // Fonts have not been parsed at this point so we can't validate
            // the font name here
            stuff_string (Movie_subtitle_font, F_NAME);
        }

        if (optional_string ("$Disable built-in translations:")) {
            stuff_boolean (&Disable_built_in_translations);
        }

        if (optional_string (
                "$Weapon shockwave damage respects huge ship flags:")) {
            stuff_boolean (&Weapon_shockwaves_respect_huge);
        }

        if (optional_string ("$Enable external default scripts:")) {
            stuff_boolean (&Enable_external_default_scripts);

            if (Enable_external_default_scripts) {
                WARNINGF (LOCATION,"Game Settings Table: Enabled external default scripts.");
            }
            else {
                WARNINGF (LOCATION,"Game Settings Table: Disabled external default scripts.");
            }
        }

        required_string ("#END");
    }
    catch (const parse::ParseException& e) {
        ERRORF (
            LOCATION, "parse failed '%s'!  Error message = %s.\n",
            (filename) ? filename : "<default game_settings.tbl>", e.what ());
        return;
    }
}

void mod_table_init () {
    mod_table_reset ();
    parse_mod_table ("game_settings.tbl");
    parse_modular_table ("*-mod.tbm", parse_mod_table);
}

bool mod_supports_version (int major, int minor, int build) {
    return Targetted_version >= gameversion::version (major, minor, build, 0);
}
void mod_table_reset () {
    Directive_wait_time = 3000;
    True_loop_argument_sexps = false;
    Fixed_turret_collisions = false;
    Damage_impacted_subsystem_first = false;
    Cutscene_camera_displays_hud = false;
    Alternate_chaining_behavior = false;
    Default_ship_select_effect = 2;
    Default_weapon_select_effect = 2;
    Default_fiction_viewer_ui = -1;
    Enable_external_shaders = false;
    Enable_external_default_scripts = false;
    Default_detail_level =
        3; // "very high" seems a reasonable default in 2012 -zookeeper
    Full_color_head_anis = false;
    Weapons_inherit_parent_collision_group = false;
    Flight_controls_follow_eyepoint_orientation = false;
    FS2NetD_port = 0;
    Briefing_window_FOV = 0.29375f;
    Disable_hc_message_ani = false;
    Red_alert_applies_to_delayed_ships = false;
    Beams_use_damage_factors = false;
    Generic_pain_flash_factor = 1.0f;
    Shield_pain_flash_factor = 0.0f;
    Targetted_version =
        gameversion::version (2, 0, 0, 0); // Defaults to retail
    Window_title = "";
    Unicode_text_mode = false;
    Movie_subtitle_font = "font01.vf";
    Enable_scripts_in_fred = false;
    Window_icon_path = "app_icon_sse";
    Disable_built_in_translations = false;
    Weapon_shockwaves_respect_huge = false;
}
