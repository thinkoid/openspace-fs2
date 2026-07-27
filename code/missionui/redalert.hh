/*
 * Copyright (C) Volition, Inc. 1999.  All rights reserved.
 *
 * All source code herein is the property of Volition, Inc. You may not sell 
 * or otherwise commercially exploit the source or things you created based on the 
 * source.
 *
*/ 

#ifndef __REDALERT_H__
#define __REDALERT_H__

#include <cfile/cfile.hh>

void	red_alert_start_mission();

void	red_alert_init();
void	red_alert_close();
void	red_alert_do_frame(float frametime);
int	red_alert_mission();
void	red_alert_set_status(int status);
int	red_alert_check_status();

void red_alert_store_wingman_status();
void red_alert_bash_wingman_status();
void red_alert_write_wingman_status(CFILE *fp);
void red_alert_read_wingman_status(CFILE *fp, int version);

#endif
