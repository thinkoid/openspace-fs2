/*
 * Copyright (C) Volition, Inc. 1999.  All rights reserved.
 *
 * All source code herein is the property of Volition, Inc. You may not sell 
 * or otherwise commercially exploit the source or things you created based on the 
 * source.
 *
*/ 

#ifndef _SHADE_H
#define _SHADE_H

extern void grx_create_shader(shader * shade, float r, float g, float b, float c );
extern void grx_set_shader( shader * shade );
extern void gr8_shade(int x,int y,int w,int h);

#endif
