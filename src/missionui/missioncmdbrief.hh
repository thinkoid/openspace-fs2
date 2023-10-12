// -*- mode: c++; -*-

#ifndef FREESPACE2_MISSIONUI_MISSIONCMDBRIEF_HH
#define FREESPACE2_MISSIONUI_MISSIONCMDBRIEF_HH

#include <defs.hh>

#define CMD_BRIEF_STAGES_MAX 10

#include <graphics/generic.hh>

struct anim;
struct anim_instance;

struct cmd_brief_stage {
    std::string text;                     // text to display
    char ani_filename[MAX_FILENAME_LEN];  // associated ani file to play
    char wave_filename[MAX_FILENAME_LEN]; // associated wav file to play
    int wave;                             // instance number of above
};

struct cmd_brief {
    int num_stages;
    cmd_brief_stage stage[CMD_BRIEF_STAGES_MAX];
    char background[GR_NUM_RESOLUTIONS][MAX_FILENAME_LEN];
};

extern cmd_brief Cmd_briefs[MAX_TVT_TEAMS];
extern cmd_brief* Cur_cmd_brief; // pointer to one of the Cmd_briefs elements
                                 // (the active one)

extern int Cmd_brief_overlay_id;

void cmd_brief_init (int stages);
void cmd_brief_close ();
void cmd_brief_do_frame (float frametime);
void cmd_brief_hold ();
void cmd_brief_unhold ();

void cmd_brief_pause ();
void cmd_brief_unpause ();

int mission_has_cmd_brief ();

#endif // FREESPACE2_MISSIONUI_MISSIONCMDBRIEF_HH
