// -*- mode: c++; -*-

#include <shared/globals.hh>
#include <hud/hud.hh>
#include <playerman/player.hh>
#include <stats/stats.hh>

#define MISSION_STATS_START_Y 80
#define ALLTIME_STATS_START_Y 270
#define MULTIPLAYER_LIST_START 20

player* Active_player;

void show_stats_init () {
    Active_player = Player;
}

// write out the label for each stat
void show_stats_label (int stage, int sx, int sy, int dy) {
    switch (stage) {
    case MISSION_STATS:
        gr_printf_menu (sx, sy, "%s", XSTR ("Mission Stats", 114));
        sy += 2 * dy;
        gr_printf_menu (sx, sy, "%s", XSTR ("Total kills", 115));
        sy += 2 * dy;
        gr_printf_menu (sx, sy, "%s", XSTR ("Primary weapon shots", 116));
        sy += dy;
        gr_printf_menu (sx, sy, "%s", XSTR ("Primary weapon hits", 117));
        sy += dy;
        gr_printf_menu (sx, sy, "%s", XSTR ("Primary friendly hits", 118));
        sy += dy;
        gr_printf_menu (sx, sy, "%s", XSTR ("Primary hit %", 119));
        sy += dy;
        gr_printf_menu (sx, sy, "%s", XSTR ("Primary friendly hit %", 120));
        sy += 2 * dy;

        gr_printf_menu (sx, sy, "%s", XSTR ("Secondary weapon shots", 121));
        sy += dy;
        gr_printf_menu (sx, sy, "%s", XSTR ("Secondary weapon hits", 122));
        sy += dy;
        gr_printf_menu (sx, sy, "%s", XSTR ("Secondary friendly hits", 123));
        sy += dy;
        gr_printf_menu (sx, sy, "%s", XSTR ("Secondary hit %", 124));
        sy += dy;
        gr_printf_menu (sx, sy, "%s", XSTR ("Secondary friendly hit %", 125));
        sy += 2 * dy;

        gr_printf_menu (sx, sy, "%s", XSTR ("Assists", 126));
        sy += 2 * dy;

        break;

    case ALL_TIME_STATS:
        gr_printf_menu (sx, sy, "%s", XSTR ("All Time Stats", 128));
        sy += 2 * dy;
        gr_printf_menu (sx, sy, "%s", XSTR ("Total kills", 115));
        sy += 2 * dy;
        gr_printf_menu (sx, sy, "%s", XSTR ("Primary weapon shots", 116));
        sy += dy;
        gr_printf_menu (sx, sy, "%s", XSTR ("Primary weapon hits", 117));
        sy += dy;
        gr_printf_menu (sx, sy, "%s", XSTR ("Primary friendly hits", 118));
        sy += dy;
        gr_printf_menu (sx, sy, "%s", XSTR ("Primary hit %", 119));
        sy += dy;
        gr_printf_menu (sx, sy, "%s", XSTR ("Primary friendly hit %", 120));
        sy += 2 * dy;

        gr_printf_menu (sx, sy, "%s", XSTR ("Secondary weapon shots", 121));
        sy += dy;
        gr_printf_menu (sx, sy, "%s", XSTR ("Secondary weapon hits", 122));
        sy += dy;
        gr_printf_menu (sx, sy, "%s", XSTR ("Secondary friendly hits", 123));
        sy += dy;
        gr_printf_menu (sx, sy, "%s", XSTR ("Secondary hit %", 124));
        sy += dy;
        gr_printf_menu (sx, sy, "%s", XSTR ("Secondary friendly hit %", 125));
        sy += 2 * dy;

        gr_printf_menu (sx, sy, "%s", XSTR ("Assists", 126));
        sy += 2 * dy;

        break;
    } // end switch
}

void stats_underline_text (int sx, int sy, char* text) {
    int w, h, fh;

    gr_get_string_size (&w, &h, text);
    fh = gr_get_font_height ();
    gr_line (sx - 1, sy + fh, sx + w + 1, sy + fh, GR_RESIZE_MENU);
}

void show_stats_numbers (int stage, int sx, int sy, int dy, int add_mission) {
    char text[30];
    float pct;

    sy += 2 * dy;
    switch (stage) {
    case MISSION_STATS:
        // mission kills stats
        sprintf (text, "%d", Active_player->stats.m_kill_count_ok);
        gr_printf_menu (sx, sy, "%s", text);
        sy += 2 * dy;
        // mission primary weapon stats
        sprintf (text, "%u", Active_player->stats.mp_shots_fired);
        gr_printf_menu (sx, sy, "%s", text);
        sy += dy;
        sprintf (text, "%u", Active_player->stats.mp_shots_hit);
        gr_printf_menu (sx, sy, "%s", text);
        sy += dy;
        sprintf (text, "%u", Active_player->stats.mp_bonehead_hits);
        gr_printf_menu (sx, sy, "%s", text);
        sy += dy;
        if (Active_player->stats.mp_shots_fired > 0)
            pct = (float)100.0 * ((float)Active_player->stats.mp_shots_hit /
                                  (float)Active_player->stats.mp_shots_fired);
        else
            pct = (float)0.0;
        sprintf (text, "%d", (int)pct);
        strcat (text, " %");
        gr_printf_menu (sx, sy, "%s", text);
        sy += dy;
        if (Active_player->stats.mp_shots_fired > 0)
            pct =
                (float)100.0 * ((float)Active_player->stats.mp_bonehead_hits /
                                (float)Active_player->stats.mp_shots_fired);
        else
            pct = (float)0.0;
        sprintf (text, "%d", (int)pct);
        strcat (text, " %");
        gr_printf_menu (sx, sy, "%s", text);
        sy += 2 * dy;

        // mission secondary weapon stats
        sprintf (text, "%u", Active_player->stats.ms_shots_fired);
        gr_printf_menu (sx, sy, "%s", text);
        sy += dy;
        sprintf (text, "%u", Active_player->stats.ms_shots_hit);
        gr_printf_menu (sx, sy, "%s", text);
        sy += dy;
        sprintf (text, "%u", Active_player->stats.ms_bonehead_hits);
        gr_printf_menu (sx, sy, "%s", text);
        sy += dy;
        if (Active_player->stats.ms_shots_fired > 0)
            pct = (float)100.0 * ((float)Active_player->stats.ms_shots_hit /
                                  (float)Active_player->stats.ms_shots_fired);
        else
            pct = (float)0.0;
        sprintf (text, "%d", (int)pct);
        strcat (text, " %");
        gr_printf_menu (sx, sy, "%s", text);
        sy += dy;
        if (Active_player->stats.ms_shots_fired > 0)
            pct =
                (float)100.0 * ((float)Active_player->stats.ms_bonehead_hits /
                                (float)Active_player->stats.ms_shots_fired);
        else
            pct = (float)0.0;
        sprintf (text, "%d", (int)pct);
        strcat (text, " %");
        gr_printf_menu (sx, sy, "%s", text);
        sy += 2 * dy;

        // mission assists and player rescues (respawns)
        sprintf (text, "%d", (int)Active_player->stats.m_assists);
        gr_printf_menu (sx, sy, "%s", text);
        sy += 2 * dy;

        break;

    case ALL_TIME_STATS:
        scoring_struct add;

        // if we are passed mission_add (the stats for the current mission),
        // copy it to "add", otherwise, leave it blank
        if (add_mission) {
            add.kill_count_ok = Active_player->stats.m_kill_count_ok;
            add.p_shots_fired = Active_player->stats.mp_shots_fired;
            add.p_shots_hit = Active_player->stats.mp_shots_hit;
            add.p_bonehead_hits = Active_player->stats.mp_bonehead_hits;

            add.s_shots_fired = Active_player->stats.ms_shots_fired;
            add.s_shots_hit = Active_player->stats.ms_shots_hit;
            add.s_bonehead_hits = Active_player->stats.ms_bonehead_hits;
        }

        // mission kills stats
        sprintf (
            text, "%d",
            Active_player->stats.kill_count_ok + add.kill_count_ok);
        hud_num_make_mono (text, font::get_current_fontnum ());
        gr_printf_menu (sx, sy, "%s", text);
        sy += 2 * dy;
        // alltime primary weapon stats
        sprintf (
            text, "%u",
            Active_player->stats.p_shots_fired + add.p_shots_fired);
        gr_printf_menu (sx, sy, "%s", text);
        sy += dy;
        sprintf (
            text, "%u", Active_player->stats.p_shots_hit + add.p_shots_hit);
        gr_printf_menu (sx, sy, "%s", text);
        sy += dy;
        sprintf (
            text, "%u",
            Active_player->stats.p_bonehead_hits + add.p_bonehead_hits);
        gr_printf_menu (sx, sy, "%s", text);
        sy += dy;
        if ((Active_player->stats.p_shots_fired + add.p_shots_fired) > 0)
            pct =
                (float)100.0 *
                ((float)(Active_player->stats.p_shots_hit + add.p_shots_hit) /
                 (float)(Active_player->stats.p_shots_fired + add.p_shots_fired));
        else
            pct = (float)0.0;
        sprintf (text, "%d", (int)pct);
        strcat (text, " %");
        gr_printf_menu (sx, sy, "%s", text);
        sy += dy;
        if ((Active_player->stats.p_bonehead_hits + add.p_bonehead_hits) > 0)
            pct =
                (float)100.0 *
                ((float)(Active_player->stats.p_bonehead_hits + add.p_bonehead_hits) /
                 (float)(Active_player->stats.p_shots_fired + add.p_shots_fired));
        else
            pct = (float)0.0;
        sprintf (text, "%d", (int)pct);
        strcat (text, " %");
        gr_printf_menu (sx, sy, "%s", text);
        sy += 2 * dy;

        // alltime secondary weapon stats
        sprintf (
            text, "%u",
            Active_player->stats.s_shots_fired + add.s_shots_fired);
        gr_printf_menu (sx, sy, "%s", text);
        sy += dy;
        sprintf (
            text, "%u", Active_player->stats.s_shots_hit + add.s_shots_hit);
        gr_printf_menu (sx, sy, "%s", text);
        sy += dy;
        sprintf (
            text, "%u",
            Active_player->stats.s_bonehead_hits + add.s_bonehead_hits);
        gr_printf_menu (sx, sy, "%s", text);
        sy += dy;
        if ((Active_player->stats.s_shots_fired + add.s_shots_fired) > 0)
            pct =
                (float)100.0 *
                ((float)(Active_player->stats.s_shots_hit + add.s_shots_hit) /
                 (float)(Active_player->stats.s_shots_fired + add.s_shots_fired));
        else
            pct = (float)0.0;
        sprintf (text, "%d", (int)pct);
        strcat (text, " %");
        gr_printf_menu (sx, sy, "%s", text);
        sy += dy;
        if ((Active_player->stats.s_bonehead_hits + add.s_bonehead_hits) > 0)
            pct =
                (float)100.0 *
                ((float)(Active_player->stats.s_bonehead_hits + add.s_bonehead_hits) /
                 (float)(Active_player->stats.s_shots_fired + add.s_shots_fired));
        else
            pct = (float)0.0;
        sprintf (text, "%d", (int)pct);
        strcat (text, " %");
        gr_printf_menu (sx, sy, "%s", text);
        sy += 2 * dy;

        // alltime assists
        sprintf (text, "%d", (int)Active_player->stats.assists + add.assists);
        gr_printf_menu (sx, sy, "%s", text);
        sy += 2 * dy;

        break;
    } // end switch
}
