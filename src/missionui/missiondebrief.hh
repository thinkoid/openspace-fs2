// -*- mode: c++; -*-

#ifndef FREESPACE2_MISSIONUI_MISSIONDEBRIEF_HH
#define FREESPACE2_MISSIONUI_MISSIONDEBRIEF_HH

#include <defs.hh>

extern int Debrief_overlay_id;

void debrief_init ();
void debrief_do_frame (float frametime);
void debrief_close ();

void debrief_disable_accept ();

void debrief_assemble_optional_mission_popup_text (
    char* buffer, char* mission_loop_desc);

void debrief_pause ();
void debrief_unpause ();

#endif // FREESPACE2_MISSIONUI_MISSIONDEBRIEF_HH
