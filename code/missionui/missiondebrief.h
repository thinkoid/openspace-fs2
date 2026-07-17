/*
 * Copyright (C) Volition, Inc. 1999.  All rights reserved.
 *
 * All source code herein is the property of Volition, Inc. You may not sell 
 * or otherwise commercially exploit the source or things you created based on the 
 * source.
 *
*/ 

#ifndef __MISSIONDEBRIEF_H__
#define __MISSIONDEBRIEF_H__

void debrief_init();
void debrief_do_frame(float frametime);
void debrief_close();

void debrief_disable_accept();
void debrief_assemble_optional_mission_popup_text(char *buffer, char *mission_loop_desc);

#endif /* __MISSIONDEBRIEF_H__ */
