// -*- mode: c++; -*-

#ifndef FREESPACE2_MISSIONUI_REDALERT_HH
#define FREESPACE2_MISSIONUI_REDALERT_HH

#include <defs.hh>

struct CFILE;

void red_alert_start_mission ();

void red_alert_init ();
void red_alert_close ();
void red_alert_do_frame (float frametime);
int red_alert_mission ();
void red_alert_invalidate_timestamp ();
int red_alert_in_progress ();
void red_alert_maybe_move_to_next_mission ();

void red_alert_store_wingman_status ();
void red_alert_bash_wingman_status ();
void red_alert_clear ();

void red_alert_voice_pause ();
void red_alert_voice_unpause ();

// should only ever be defined in redalert.cpp and the pilot file code!!
#ifdef REDALERT_INTERNAL

#define RED_ALERT_WARN_TIME \
    4000 // time to warn user that new orders are coming

static const int RED_ALERT_DESTROYED_SHIP_CLASS = -1;
static const int RED_ALERT_PLAYER_DEL_SHIP_CLASS = -2;
static const int RED_ALERT_LOWEST_VALID_SHIP_CLASS =
    RED_ALERT_PLAYER_DEL_SHIP_CLASS; // for ship index bounds checks

struct red_alert_ship_status  {
    std::string name;
    float hull;
    int ship_class;
    std::vector< float > subsys_current_hits;
    std::vector< float > subsys_aggregate_current_hits;
    std::vector< wep_t > primary_weapons;
    std::vector< wep_t > secondary_weapons;
};

extern std::vector< red_alert_ship_status > Red_alert_wingman_status;
extern std::string Red_alert_precursor_mission;
#endif

#endif // FREESPACE2_MISSIONUI_REDALERT_HH
