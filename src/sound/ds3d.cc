/*
 * Copyright (C) Volition, Inc. 1999.  All rights reserved.
 *
 * All source code herein is the property of Volition, Inc. You may not sell
 * or otherwise commercially exploit the source or things you created based on the
 * source.
 *
*/

// DirectSound3D listener/buffer updates rebuilt on the OpenAL listener and
// per-source position attributes.

#include <globalincs/pstypes.hh>
// (windows.h removed)
#include <sound/ds3d.hh>
#include <sound/ds.hh>
#include <sound/channel.hh>

#include <AL/al.h>

int DS3D_inited = FALSE;


// ---------------------------------------------------------------------------------------
// ds3d_update_buffer()
//
//	parameters:		channel	=> identifies the 3D sound to update
//						min		=>	the distance at which sound doesn't get any louder
//						max		=>	the distance at which sound doesn't attenuate any further
//						pos		=> world position of sound
//						vel		=> velocity of the objects producing the sound
//
//	returns:		0		=>		success
//					-1		=>		failure
//
//
int ds3d_update_buffer(int channel, float min, float max, vector *pos, vector *vel)
{
	if (DS3D_inited == FALSE)
		return 0;

	if ( channel == -1 )
		return 0;

	// set the min distance
	OpenAL_ErrorPrint( alSourcef(Channels[channel].source_id, AL_REFERENCE_DISTANCE, min) );

	// set the max distance
	OpenAL_ErrorPrint( alSourcef(Channels[channel].source_id, AL_MAX_DISTANCE, max) );

	// set rolloff factor
	OpenAL_ErrorPrint( alSourcef(Channels[channel].source_id, AL_ROLLOFF_FACTOR, 1.0f) );

	// set the buffer position
	if ( pos != NULL ) {
		ALfloat alpos[] = { pos->x, pos->y, pos->z };
		OpenAL_ErrorPrint( alSourcefv(Channels[channel].source_id, AL_POSITION, alpos) );
	}

	// set the buffer velocity
	if ( vel != NULL ) {
		ALfloat alvel[] = { vel->x, vel->y, vel->z };
		OpenAL_ErrorPrint( alSourcefv(Channels[channel].source_id, AL_VELOCITY, alvel) );
	} else {
		ALfloat alvel[] = { 0.0f, 0.0f, 0.0f };
		OpenAL_ErrorPrint( alSourcefv(Channels[channel].source_id, AL_VELOCITY, alvel) );
	}

	return 0;
}


// ---------------------------------------------------------------------------------------
// ds3d_update_listener()
//
//	returns:		0		=>		success
//					-1		=>		failure
//
int ds3d_update_listener(vector *pos, vector *vel, matrix *orient)
{
	if (DS3D_inited == FALSE)
		return 0;

	// set the listener position
	if ( pos != NULL ) {
		OpenAL_ErrorPrint( alListener3f(AL_POSITION, pos->x, pos->y, pos->z) );
	}

	// set the listener velocity
	if ( vel != NULL ) {
		OpenAL_ErrorPrint( alListener3f(AL_VELOCITY, vel->x, vel->y, vel->z) );
	}

	// set the listener orientation
	if ( orient != NULL ) {
		// fvec is the at/front vector, uvec is the up/top vector
		ALfloat list_orien[] = { orient->fvec.x, orient->fvec.y, orient->fvec.z,
									orient->uvec.x, orient->uvec.y, orient->uvec.z };
		OpenAL_ErrorPrint( alListenerfv(AL_ORIENTATION, list_orien) );
	}

	return 0;
}

// ---------------------------------------------------------------------------------------
// ds3d_set_sound_cone()
//
//	returns:		0		=>		success
//					-1		=>		failure
//
int ds3d_set_sound_cone(int channel, int inner_angle, int outer_angle, int vol)
{
	if (DS3D_inited == FALSE)
		return 0;

	OpenAL_ErrorPrint( alSourcei(Channels[channel].source_id, AL_CONE_INNER_ANGLE, inner_angle) );
	OpenAL_ErrorPrint( alSourcei(Channels[channel].source_id, AL_CONE_OUTER_ANGLE, outer_angle) );
	OpenAL_ErrorPrint( alSourcef(Channels[channel].source_id, AL_CONE_OUTER_GAIN, ds_get_percentage_vol(vol)) );

	return 0;
}

// ---------------------------------------------------------------------------------------
// ds3d_init()
//
// Initialize the positional sound system.
//
// returns:     -1	=> init failed
//              0		=> success
int ds3d_init(int voice_manager_required)
{
	if ( DS3D_inited == TRUE )
		return 0;

	// the listener itself is set up in ds_init()

	DS3D_inited = TRUE;
	return 0;
}


// ---------------------------------------------------------------------------------------
// ds3d_close()
//
// De-initialize the positional sound system
//
void ds3d_close()
{
	if ( DS3D_inited == FALSE )
		return;

	DS3D_inited = FALSE;
}
