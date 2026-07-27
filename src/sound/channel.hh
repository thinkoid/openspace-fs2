/*
 * Copyright (C) Volition, Inc. 1999.  All rights reserved.
 *
 * All source code herein is the property of Volition, Inc. You may not sell
 * or otherwise commercially exploit the source or things you created based on the
 * source.
 *
*/


// (windows.h removed)
// (vdsound.h removed)

#ifndef __CHANNEL_H__
#define __CHANNEL_H__

#include <AL/al.h>

#include <sound/ds.hh>

typedef struct channel
{
	int		sig;			// uniquely identifies the sound playing on the channel
	int		snd_id;		// identifies which kind of sound is playing
	ALuint	source_id;	// OpenAL source the sound plays through
	int		buf_id;		// index into sound_buffers[] currently bound, or -1
	int		looping;		// flag to indicate that the sound is looping
	int		vol;			// in DirectSound units
	int		priority;	// implementation dependant priority
	bool		is_voice_msg;
	DWORD	last_position;
} channel;


// #define	MAX_CHANNELS  16
extern	channel* Channels;    //[MAX_CHANNELS];

#endif /* __CHANNEL_H__ */
