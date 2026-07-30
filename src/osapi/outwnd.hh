/*
 * Copyright (C) Volition, Inc. 1999.  All rights reserved.
 *
 * All source code herein is the property of Volition, Inc. You may not sell 
 * or otherwise commercially exploit the source or things you created based on the 
 * source.
 *
*/

#ifndef _OUTWND_H
#define _OUTWND_H

void outwnd_init(int display_under_freespace_window = 0);
void outwnd_close();
void outwnd_printf(const char *id, const char *format, ...);
void outwnd_printf2(const char *format, ...);

#endif
