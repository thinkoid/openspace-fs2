/*
 * Copyright (C) Volition, Inc. 1999.  All rights reserved.
 *
 * All source code herein is the property of Volition, Inc. You may not sell 
 * or otherwise commercially exploit the source or things you created based on the 
 * source.
 *
*/ 

#define CMD_BRIEF_TEXT_MAX		16384
#define CMD_BRIEF_STAGES_MAX	10

typedef struct {
	char *text;  // text to display
	char ani_filename[MAX_FILENAME_LEN];  // associated ani file to play
	anim *anim;
	anim_instance *anim_instance;
	int anim_ref;  // potential reference to another index (use it's anim instead of this's)
	char wave_filename[MAX_FILENAME_LEN];
	int wave;  // instance number of above
} cmd_brief_stage;

typedef struct {
	int num_stages;
	cmd_brief_stage stage[CMD_BRIEF_STAGES_MAX];
} cmd_brief;

extern cmd_brief Cmd_briefs[MAX_TEAMS];
extern cmd_brief *Cur_cmd_brief;  // pointer to one of the Cmd_briefs elements (the active one)

void cmd_brief_init(int stages);
void cmd_brief_close();
void cmd_brief_do_frame(float frametime);
void cmd_brief_hold();
void cmd_brief_unhold();
