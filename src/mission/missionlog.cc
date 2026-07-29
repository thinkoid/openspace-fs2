/*
 * Copyright (C) Volition, Inc. 1999.  All rights reserved.
 *
 * All source code herein is the property of Volition, Inc. You may not sell 
 * or otherwise commercially exploit the source or things you created based on the 
 * source.
 *
*/

#include <io/timer.hh>
#include <parse/parselo.hh>
#include <math/fix.hh>
#include <gamesequence/gamesequence.hh>
#include <freespace2/freespace.hh>
#include <mission/missionlog.hh>
#include <mission/missionparse.hh>
#include <io/key.hh>
#include <graphics/2d.hh>
#include <graphics/font.hh>
#include <mission/missiongoals.hh>
#include <globalincs/alphacolors.hh>
#include <localization/localize.hh>

#define MAX_LOG_ENTRIES 700
#define MAX_LOG_LINES 1000

// used for high water mark for culling out log entries
#define LOG_CULL_MARK ((int)(MAX_LOG_ENTRIES * 0.95f))
#define LOG_CULL_DOORDIE_MARK ((int)(MAX_LOG_ENTRIES * 0.99f))
#define LOG_LAST_DITCH_CULL_NUM ((int)(MAX_LOG_ENTRIES * 0.20f))
#define LOG_HALFWAY_REPORT_NUM ((int)(MAX_LOG_ENTRIES * 0.50f))

#define ML_FLAG_PRIMARY 1
#define ML_FLAG_SECONDARY 2

#define EMPTY_LOG_NAME ""

// defines for X position offsets of different items for mission log
#define TIME_X 10
#define OBJECT_X 75
#define ACTION_X 250

#define LOG_COLOR_NORMAL 0
#define LOG_COLOR_BRIGHT 1
#define LOG_COLOR_FRIENDLY 2
#define LOG_COLOR_HOSTILE 3
#define LOG_COLOR_OTHER 4

// defines for log flags
#define LOG_FLAG_GOAL_FAILED (1 << 0)
#define LOG_FLAG_GOAL_TRUE (1 << 1)

typedef struct log_text_seg
{
    log_text_seg *next; // linked list
    char *text; // the text
    int color; // color text should be displayed in
    int x; // x offset to display text at
    int flags; // used to possibly print special characters when displaying the log
} log_text_seg;

int Num_log_lines;
static int X, P_width;

// Log_lines is used for scrollback display purposes.
static log_text_seg *Log_lines[MAX_LOG_LINES];
static int Log_line_timestamps[MAX_LOG_LINES];

log_entry log_entries[MAX_LOG_ENTRIES]; // static array because John says....
int last_entry;

void
mission_log_init()
{
    last_entry = 0;

    // zero out all the memory so we don't get bogus information when playing across missions!
    memset(log_entries, 0, sizeof(log_entries));
}

// returns the number of entries in the mission log
int
mission_log_query_scrollback_size()
{
    return last_entry;
}

// function to clean up the mission log removing obsolete entries.  Entries might get marked obsolete
// in several ways -- having to recycle entries, a ship's subsystem destroyed entries when a ship is
// fully destroyed, etc.
void
mission_log_cull_obsolete_entries()
{
    int i, index;

    nprintf(("missionlog", "culling obsolete entries.  starting last entry %d.\n",
             last_entry));
    // find the first obsolete entry
    for (i = 0; i < last_entry; i++)
        if (log_entries[i].flags & MLF_OBSOLETE)
            break;

    // nothing to do if next if statement is true
    if (i == last_entry)
        return;

    // compact the log array, removing the obsolete entries.
    index = i; // index is the first obsolete entry

    // 'index' should always point to the next element in the list
    // which is getting compacted.  'i' points to the next array
    // element to be replaced.
    do {
        // get to the next non-obsolete entry.  The obsolete entry must not be essential either!
        while ((log_entries[index].flags & MLF_OBSOLETE) &&
               !(log_entries[index].flags & MLF_ESSENTIAL)) {
            index++;
            last_entry--;
        }

        log_entries[i++] = log_entries[index++];
    } while (i < last_entry);

#ifndef NDEBUG
    nprintf(("missionlog", "Ending entry: %d.\n", last_entry));
#endif
}

// function to mark entries as obsolete.  Passed is the type of entry that is getting added
// to the log.  Some entries might get marked obsolete as a result of this type
void
mission_log_obsolete_entries(int type, char *pname)
{
    int i;
    log_entry *entry = NULL;

    // before adding this entry, check to see if the entry type is a ship destroyed entry.
    // If so, we can remove any subsystem destroyed entries from the log for this ship.
    if (type == LOG_SHIP_DESTROYED) {
        for (i = 0; i < last_entry; i++) {
            entry = &log_entries[i];

            // check to see if the type is a subsystem destroyed entry, and that it belongs to the
            // ship passed into this routine.  If it matches, mark as obsolete.  We'll clean up
            // the log when it starts to get full
            if (!stricmp(pname, entry->pname)) {
                if ((entry->type == LOG_SHIP_SUBSYS_DESTROYED) ||
                    (entry->type == LOG_SHIP_DISARMED) ||
                    (entry->type == LOG_SHIP_DISABLED))
                    entry->flags |= MLF_OBSOLETE;
            }
        }
    }

    // check to see if we are getting to about 80% of our log capacity.  If so, cull the log.
    if (last_entry > LOG_CULL_MARK) {
        mission_log_cull_obsolete_entries();

        // if we culled the entries, and we are still low on space, we need to take more drastic measures.
        // these include removing all non-essential entries from the log.  These entries are entries
        // which has not been asked for by mission_log_get_time
        if (last_entry > LOG_CULL_MARK) {
            nprintf(
                ("missionlog",
                 "marking the first %d non-essential log entries as obsolete\n",
                 LOG_LAST_DITCH_CULL_NUM));
            for (i = 0; i < LOG_LAST_DITCH_CULL_NUM; i++) {
                entry = &log_entries[i];
                if (!(entry->flags & MLF_ESSENTIAL)) {
                    entry->flags |= MLF_OBSOLETE;
                }
            }

            // cull the obsolete entries again
            mission_log_cull_obsolete_entries();

            // if we get to this point, and there are no entries left -- we are in big trouble.  We will simply
            // mark the first 20% of the log as obsolete and compress.  Don't do this unless we are *really*
            // in trouble
            if (last_entry > LOG_CULL_DOORDIE_MARK) {
                nprintf(("missionlog",
                         "removing the first %d entries in the mission log!!!!\n",
                         LOG_LAST_DITCH_CULL_NUM));
                for (i = 0; i < LOG_LAST_DITCH_CULL_NUM; i++) {
                    entry->flags |= MLF_OBSOLETE;
                }

                mission_log_cull_obsolete_entries();
            }
        }
    }
}

// assigns flag values to an entry based on the team value passed in
void
mission_log_flag_team(log_entry *entry, int which_entry, int team)
{
    if (which_entry == ML_FLAG_PRIMARY) {
        if (team == TEAM_FRIENDLY)
            entry->flags |= MLF_PRIMARY_FRIENDLY;
        else
            entry->flags |= MLF_PRIMARY_HOSTILE;
    }
    else if (which_entry == ML_FLAG_SECONDARY) {
        if (team == TEAM_FRIENDLY)
            entry->flags |= MLF_SECONDARY_FRIENDLY;
        else
            entry->flags |= MLF_SECONDARY_HOSTILE;
    }
    else
        Int3(); // get allender -- impossible type
}

// following function adds an entry into the mission log.
// pass a type and a string which indicates the object
// that this event is for.
void
mission_log_add_entry(int type, char *pname, char *sname, int info_index)
{
    int last_entry_save;
    log_entry *entry;

    last_entry_save = last_entry;

    // mark any entries as obsolete.  Part of the pruning is done based on the type (and name) passed
    // for a new entry
    mission_log_obsolete_entries(type, pname);

    entry = &log_entries[last_entry];

    if (last_entry == MAX_LOG_ENTRIES) {
        return;
    }

    entry->type = type;
    if (pname) {
        Assert(strlen(pname) < NAME_LENGTH);
        strcpy(entry->pname, pname);
    }
    else
        strcpy(entry->pname, EMPTY_LOG_NAME);

    if (sname) {
        Assert(strlen(sname) < NAME_LENGTH);
        strcpy(entry->sname, sname);
    }
    else
        strcpy(entry->sname, EMPTY_LOG_NAME);

    entry->index = info_index;
    entry->flags = 0;

    // determine the contents of the flags member based on the type of entry we added.  We need to store things
    // like team for the primary and (possibly) secondary object for this entry.
    switch (type) {
        int index, si;

    case LOG_SHIP_DESTROYED:
    case LOG_SHIP_ARRIVE:
    case LOG_SHIP_DEPART:
    case LOG_SHIP_DOCK:
    case LOG_SHIP_SUBSYS_DESTROYED:
    case LOG_SHIP_UNDOCK:
    case LOG_SHIP_DISABLED:
    case LOG_SHIP_DISARMED:
    case LOG_SELF_DESTRUCT:
        index = ship_name_lookup(pname);

        Assert(index != -1);
        if (index < 0) {
            mission_log_flag_team(entry, ML_FLAG_PRIMARY, TEAM_FRIENDLY);
        }
        else {
            mission_log_flag_team(entry, ML_FLAG_PRIMARY, Ships[index].team);
        }

        // some of the entries have a secondary component.  Figure out what is up with them.
        if ((type == LOG_SHIP_DOCK) || (type == LOG_SHIP_UNDOCK)) {
            if (sname) {
                index = ship_name_lookup(sname);
                Assert(index != -1);
                mission_log_flag_team(entry, ML_FLAG_SECONDARY,
                                      Ships[index].team);
            }
        }
        else if (type == LOG_SHIP_DESTROYED) {
            if (sname) {
                int team;

                index = ship_name_lookup(sname);
                // no ship, then it probably exited -- check the exited
                if (index == -1) {
                    index = ship_find_exited_ship_by_name(sname);
                    if (index == -1) {
                        // Int3();     // get allender.  name of object who killed ship appears to be bogus!!!
                        break;
                    }
                    team = Ships_exited[index].team;
                }
                else {
                    team = Ships[index].team;
                }

                mission_log_flag_team(entry, ML_FLAG_SECONDARY, team);
            }
            else {
                nprintf(("missionlog",
                         "No secondary name for ship destroyed log entry!\n"));
            }
        }
        else if ((type == LOG_SHIP_SUBSYS_DESTROYED) &&
                 (Ship_info[Ships[index].ship_info_index].flags &
                  SIF_SMALL_SHIP)) {
            // make subsystem destroyed entries for small ships hidden
            entry->flags |= MLF_HIDDEN;
        }
        else if ((type == LOG_SHIP_ARRIVE) && (Ships[index].wingnum != -1)) {
            // arrival of ships in wings don't display
            entry->flags |= MLF_HIDDEN;
        }
        break;

    case LOG_WING_DESTROYED:
    case LOG_WING_DEPART:
    case LOG_WING_ARRIVE:
        index = wing_name_lookup(pname, 1);
        Assert(index != -1);
        Assert(info_index != -1); // this is the team value

        // get the team value for this wing.  Departed or destroyed wings will pass the team
        // value in info_index parameter.  For arriving wings, get the team value from the
        // first ship in the list
        if (type == LOG_WING_ARRIVE) {
            si = Wings[index].ship_index[0];
            Assert(si != -1);
            mission_log_flag_team(entry, ML_FLAG_PRIMARY, Ships[si].team);
        }
        else {
            mission_log_flag_team(entry, ML_FLAG_PRIMARY, info_index);
        }

#ifndef NDEBUG
        // MWA 2/25/98.  debug code to try to find any ships in this wing that have departed.
        // scan through all log entries and find at least one ship_depart entry for a ship
        // that was in this wing.
        if (type == LOG_WING_DEPART) {
            int i;

            // if all were destroyed, then don't do this debug code.
            if (Wings[index].total_destroyed ==
                Wings[index].total_arrived_count) {
                break;
            }

            for (i = 0; i < last_entry; i++) {
                if (log_entries[i].type != LOG_SHIP_DEPART) {
                    continue;
                }
                if (log_entries[i].index == index) {
                    break;
                }
            }
            if (i == last_entry) {
                Int3(); // get Allender -- cannot find any departed ships from wing that supposedly departed.
            }
        }
#endif

        break;

        // don't display waypoint done entries
    case LOG_WAYPOINTS_DONE:
        entry->flags |= MLF_HIDDEN;
        break;

    default:
        break;
    }

    entry->timestamp = Missiontime;

    last_entry++;

#ifndef NDEBUG
    if (!(last_entry % 10)) {
        if ((last_entry > LOG_HALFWAY_REPORT_NUM) &&
            (last_entry > last_entry_save)) {
            nprintf(
                ("missionlog",
                 "new highwater point reached for mission log (%d entries).\n",
                 last_entry));
        }
    }
#endif
}

// function to determine is the given event has taken place count number of times.

int
mission_log_get_time_indexed(int type, char *pname, char *sname, int count,
                             fix *time)
{
    int i, found;
    log_entry *entry;

    entry = &log_entries[0];
    for (i = 0; i < last_entry; i++) {
        found = 0;
        if (entry->type == type) {
            // if we are looking for a dock/undock entry, then we don't care about the order in which the names
            // were passed into this function.  Count the entry as found if either name matches both in the other
            // set.
            if ((type == LOG_SHIP_DOCK) || (type == LOG_SHIP_UNDOCK)) {
                Assert(sname);
                if ((!stricmp(entry->pname, pname) &&
                     !stricmp(entry->sname, sname)) ||
                    (!stricmp(entry->pname, sname) &&
                     !stricmp(entry->sname, pname)))
                    found = 1;
            }
            else {
                // for non dock/undock goals, then the names are important!
                if (stricmp(entry->pname, pname))
                    goto next_entry;
                if (!sname || !stricmp(sname, entry->sname))
                    found = 1;
            }

            if (found) {
                count--;
                if (!count) {
                    entry->flags |=
                        MLF_ESSENTIAL; // since the goal code asked for this entry, mark it as essential
                    if (time)
                        *time = entry->timestamp;
                    return 1;
                }
            }
        }

    next_entry:
        entry++;
    }

    return 0;
}

// this function determines if the given type of event on the specified
// object has taken place yet.  If not, it returns 0.  If it has, the
// timestamp that the event happened is returned in the time parameter
int
mission_log_get_time(int type, char *pname, char *sname, fix *time)
{
    return mission_log_get_time_indexed(type, pname, sname, 1, time);
}

void
message_log_add_seg(int n, int x, int color, const char *text, int flags = 0)
{
    log_text_seg *seg, **parent;

    if ((n < 0) || (n >= MAX_LOG_LINES))
        return;

    parent = &Log_lines[n];
    while (*parent)
        parent = &((*parent)->next);

    seg = (log_text_seg *)malloc(sizeof(log_text_seg));
    Assert(seg);
    seg->text = strdup(text);
    seg->color = color;
    seg->x = x;
    seg->flags = flags;
    seg->next = NULL;
    *parent = seg;
}

void
message_log_add_segs(char *text, int color, int flags = 0)
{
    char *ptr;
    int w;

    while (1) {
        if (X == ACTION_X) {
            while (is_white_space(*text))
                text++;
        }

        if (!text) {
            mprintf((
                "Why are you passing a NULL pointer to message_log_add_segs?\n"));
            return;
        }

        if (!text[0]) {
            return;
        }

        if (P_width - X < 1)
            ptr = text;
        else
            ptr = split_str_once(text, P_width - X);

        if (ptr != text)
            message_log_add_seg(Num_log_lines, X, color, text, flags);

        if (!ptr) {
            gr_get_string_size(&w, NULL, text);
            X += w;
            return;
        }

        Num_log_lines++;
        X = ACTION_X;
        text = ptr;
    }
}

void
message_log_remove_segs(int n)
{
    log_text_seg *ptr, *ptr2;

    if ((n < 0) || (n >= MAX_LOG_LINES))
        return;

    ptr = Log_lines[n];
    while (ptr) {
        ptr2 = ptr->next;
        free(ptr);
        ptr = ptr2;
    }

    Log_lines[n] = NULL;
}

// pw = total pixel width
void
message_log_init_scrollback(int pw)
{
    char text[256];
    log_entry *entry;
    int i, c, kill, type;

    P_width = pw;
    mission_log_cull_obsolete_entries(); // compact array so we don't have gaps

    // initialize the log lines data
    Num_log_lines = 0;
    for (i = 0; i < MAX_LOG_LINES; i++) {
        Log_lines[i] = NULL;
        Log_line_timestamps[i] = 0;
    }

    for (i = 0; i < last_entry; i++) {
        entry = &log_entries[i];

        if (entry->flags & MLF_HIDDEN)
            continue;

        // track time of event (normal timestamp milliseconds format)
        Log_line_timestamps[Num_log_lines] = (int)(f2fl(entry->timestamp) *
                                                   1000.0f);

        // Generate subject ship text for entry
        if (entry->flags & MLF_PRIMARY_FRIENDLY) {
            c = LOG_COLOR_FRIENDLY;
        }
        else if (entry->flags & MLF_PRIMARY_HOSTILE) {
            c = LOG_COLOR_HOSTILE;
        }
        else if ((entry->type == LOG_GOAL_SATISFIED) ||
                 (entry->type == LOG_GOAL_FAILED)) {
            c = LOG_COLOR_BRIGHT;
        }
        else {
            c = LOG_COLOR_OTHER;
        }

        if ((Lcl_gr) && ((entry->type == LOG_GOAL_FAILED) ||
                         (entry->type == LOG_GOAL_SATISFIED))) {
            // in german goal events, just say "objective" instead of objective name
            // this is cuz we cant translate objective names
            message_log_add_seg(Num_log_lines, OBJECT_X, c, "Einsatzziel");
        }
        else {
            message_log_add_seg(Num_log_lines, OBJECT_X, c, entry->pname);
        }

        // now on to the actual message itself
        X = ACTION_X;
        kill = 0;
        if (entry->flags & MLF_SECONDARY_FRIENDLY) {
            c = LOG_COLOR_FRIENDLY;
        }
        else if (entry->flags & MLF_SECONDARY_HOSTILE) {
            c = LOG_COLOR_HOSTILE;
        }
        else {
            c = LOG_COLOR_NORMAL;
        }

        switch (entry->type) {
        case LOG_SHIP_DESTROYED:
            message_log_add_segs(XSTR("Destroyed", 404), LOG_COLOR_NORMAL);
            if (strlen(entry->sname)) {
                message_log_add_segs(XSTR("  Kill: ", 405), LOG_COLOR_NORMAL);
                message_log_add_segs(entry->sname, c);
                if (entry->index >= 0) {
                    sprintf(text, NOX(" (%d%%)"), entry->index);
                    message_log_add_segs(text, LOG_COLOR_BRIGHT);
                }
            }
            break;

        case LOG_SELF_DESTRUCT:
            message_log_add_segs(XSTR("Self Destructed", 1476), LOG_COLOR_NORMAL);
            break;

        case LOG_WING_DESTROYED:
            message_log_add_segs(XSTR("Destroyed", 404), LOG_COLOR_NORMAL);
            break;

        case LOG_SHIP_ARRIVE:
            message_log_add_segs(XSTR("Arrived", 406), LOG_COLOR_NORMAL);
            break;

        case LOG_WING_ARRIVE:
            if (entry->index > 1) {
                sprintf(text, XSTR("Arrived (wave %d)", 407), entry->index);
            }
            else {
                strcpy(text, XSTR("Arrived", 406));
            }
            message_log_add_segs(text, LOG_COLOR_NORMAL);
            break;

        case LOG_SHIP_DEPART:
            message_log_add_segs(XSTR("Departed", 408), LOG_COLOR_NORMAL);
            break;

        case LOG_WING_DEPART:
            message_log_add_segs(XSTR("Departed", 408), LOG_COLOR_NORMAL);
            break;

        case LOG_SHIP_DOCK:
            message_log_add_segs(XSTR("docked with ", 409), LOG_COLOR_NORMAL);
            message_log_add_segs(entry->sname, c);
            break;

        case LOG_SHIP_SUBSYS_DESTROYED: {
            int si_index, model_index;

            si_index = (int)((entry->index >> 16) & 0xffff);
            model_index = (int)(entry->index & 0xffff);

            message_log_add_segs(XSTR("Subsystem ", 410), LOG_COLOR_NORMAL);
            //message_log_add_segs(entry->sname, LOG_COLOR_BRIGHT);
            char *subsys_name = Ship_info[si_index].subsystems[model_index].name;
            if (Ship_info[si_index].subsystems[model_index].type ==
                SUBSYSTEM_TURRET) {
                subsys_name = XSTR("Turret", 1487);
            }
            message_log_add_segs(subsys_name, LOG_COLOR_BRIGHT);
            message_log_add_segs(XSTR(" destroyed", 411), LOG_COLOR_NORMAL);
            break;
        }

        case LOG_SHIP_UNDOCK:
            message_log_add_segs(XSTR("Undocked with ", 412), LOG_COLOR_NORMAL);
            message_log_add_segs(entry->sname, c);
            break;

        case LOG_SHIP_DISABLED:
            message_log_add_segs(XSTR("Disabled", 413), LOG_COLOR_NORMAL);
            break;

        case LOG_SHIP_DISARMED:
            message_log_add_segs(XSTR("Disarmed", 414), LOG_COLOR_NORMAL);
            break;

        case LOG_PLAYER_REARM:
            message_log_add_segs(XSTR(" called for rearm", 415),
                                 LOG_COLOR_NORMAL);
            break;

        case LOG_PLAYER_REARM_ABORT:
            message_log_add_segs(XSTR(" aborted rearm", 416), LOG_COLOR_NORMAL);
            break;

        case LOG_PLAYER_REINFORCEMENT:
            message_log_add_segs(XSTR("Called in as reinforcement", 417),
                                 LOG_COLOR_NORMAL);
            break;

        case LOG_CARGO_REVEALED:
            Assert(entry->index != -1);
            message_log_add_segs(XSTR("Cargo revealed: ", 418), LOG_COLOR_NORMAL);
            message_log_add_segs(Cargo_names[entry->index], LOG_COLOR_BRIGHT);
            break;

        case LOG_CAP_SUBSYS_CARGO_REVEALED:
            Assert(entry->index != -1);
            message_log_add_segs(entry->sname, LOG_COLOR_NORMAL);
            message_log_add_segs(XSTR(" subsystem cargo revealed: ", 1488),
                                 LOG_COLOR_NORMAL);
            message_log_add_segs(Cargo_names[entry->index], LOG_COLOR_BRIGHT);
            break;

        case LOG_GOAL_SATISFIED:
        case LOG_GOAL_FAILED: {
            type = Mission_goals[entry->index].type & GOAL_TYPE_MASK;

            // don't display failed bonus goals
            if ((type == BONUS_GOAL) && (entry->type == LOG_GOAL_FAILED)) {
                kill = 1;
                break; // don't display this line
            }

            sprintf(text, XSTR("%s objective ", 419), Goal_type_text(type));
            if (entry->type == LOG_GOAL_SATISFIED)
                strcat(text, XSTR("satisfied.", 420));
            else
                strcat(text, XSTR("failed.", 421));

            message_log_add_segs(text, LOG_COLOR_BRIGHT,
                                 (entry->type == LOG_GOAL_SATISFIED
                                      ? LOG_FLAG_GOAL_TRUE
                                      : LOG_FLAG_GOAL_FAILED));
            break;
        } // matches case statement!
        }

        if (kill) {
            message_log_remove_segs(Num_log_lines);
        }
        else {
            if (Num_log_lines < MAX_LOG_LINES)
                Num_log_lines++;
        }
    }
}

void
message_log_shutdown_scrollback()
{
    int i;

    for (i = 0; i < MAX_LOG_LINES; i++)
        message_log_remove_segs(i);

    Num_log_lines = 0;
}

// message_log_scrollback displays the contents of the mesasge log currently much like the HUD
// message scrollback system.  I'm sure this system will be overhauled.
void
mission_log_scrollback(int line, int list_x, int list_y, int list_w, int list_h)
{
    char buf[256];
    int y;
    int font_h = gr_get_font_height();
    log_text_seg *seg;

    y = 0;
    while (y + font_h <= list_h) {
        if (line >= Num_log_lines)
            break;

        if (Log_line_timestamps[line]) {
            gr_set_color_fast(&Color_text_normal);
            gr_print_timestamp(list_x + TIME_X, list_y + y,
                               Log_line_timestamps[line]);
        }

        seg = Log_lines[line];
        while (seg) {
            switch (seg->color) {
            case LOG_COLOR_BRIGHT:
                gr_set_color_fast(&Color_bright);
                break;

            case LOG_COLOR_FRIENDLY:
                gr_set_color_fast(&Color_bright_green);
                break;

            case LOG_COLOR_HOSTILE:
                gr_set_color_fast(&Color_bright_red);
                break;

            case LOG_COLOR_OTHER:
                gr_set_color_fast(&Color_normal);
                break;

            default:
                gr_set_color_fast(&Color_text_normal);
                break;
            }

            strcpy(buf, seg->text);
            if (seg->x < ACTION_X)
                gr_force_fit_string(buf, 256, ACTION_X - OBJECT_X - 8);
            else
                gr_force_fit_string(buf, 256, list_w - seg->x);

            gr_string(list_x + seg->x, list_y + y, buf);

            // possibly "print" some symbols for interesting log entries
            if ((seg->flags & LOG_FLAG_GOAL_TRUE) ||
                (seg->flags & LOG_FLAG_GOAL_FAILED)) {
                int i;

                if (seg->flags & LOG_FLAG_GOAL_FAILED)
                    gr_set_color_fast(&Color_bright_red);
                else
                    gr_set_color_fast(&Color_bright_green);

                i = list_y + y + font_h / 2 - 1;
                gr_circle(list_x + TIME_X - 6, i, 5);

                gr_set_color_fast(&Color_bright);
                gr_line(list_x + TIME_X - 10, i, list_x + TIME_X - 8, i);
                gr_line(list_x + TIME_X - 6, i - 4, list_x + TIME_X - 6, i - 2);
                gr_line(list_x + TIME_X - 4, i, list_x + TIME_X - 2, i);
                gr_line(list_x + TIME_X - 6, i + 2, list_x + TIME_X - 6, i + 4);
            }

            seg = seg->next;
        }

        y += font_h;
        line++;
    }
}
