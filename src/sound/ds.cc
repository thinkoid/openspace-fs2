/*
 * Copyright (C) Volition, Inc. 1999.  All rights reserved.
 *
 * All source code herein is the property of Volition, Inc. You may not sell
 * or otherwise commercially exploit the source or things you created based on the
 * source.
 *
*/

// The DirectSound backend rebuilt on OpenAL: secondary buffers become AL
// buffers, playing channels become AL sources.  The channel-pool logic
// (instance limits, lowest-volume eviction) is retail's; the AL mechanics
// follow the fs2open Linux port (2004-2010 line).

#include <globalincs/pstypes.hh>
// (windows.h removed)
#include <cfile/cfile.hh>
#include <sound/ds.hh>
#include <sound/channel.hh>
#include <sound/ds3d.hh>
#include <sound/acm.hh>
#include <osapi/osapi.hh>
// (dscap.h removed -- voice capture is multiplayer-only, stubbed)

#include <AL/al.h>
#include <AL/alc.h>

channel* Channels;		//[MAX_CHANNELS];
static int channel_next_sig = 1;

#define MAX_DS_SOFTWARE_BUFFERS	256
typedef struct sound_buffer
{
	ALuint	buf_id;			// OpenAL buffer id (0 means slot free)
	int		source_id;		// index of the channel currently playing this buffer, or -1

	int		frequency;
	int		bits_per_sample;
	int		nchannels;
	int		nseconds;
	int		nbytes;
} sound_buffer;

sound_buffer sound_buffers[MAX_DS_SOFTWARE_BUFFERS];

int ds_vol_lookup[101];						// lookup table for direct sound volumes
int ds_initialized = FALSE;

extern int Snd_sram;					// mem (in bytes) used up by storing sounds in system memory

static int Ds_use_ds3d = 0;
static int Ds_use_a3d = 0;
static int Ds_use_eax = 0;

static int MAX_CHANNELS = 32;		// initialized properly in ds_init_channels()

static ALCdevice	*ds_sound_device = NULL;
static ALCcontext	*ds_sound_context = NULL;

//--------------------------------------------------------------------------
// openal_error_string()
//
// Returns the human readable error string if there is an error or NULL if not
//
const char *openal_error_string(int get_alc)
{
	int i;

	if (get_alc) {
		i = alcGetError(ds_sound_device);

		if ( i != ALC_NO_ERROR )
			return (const char *)alcGetString(NULL, i);
	}
	else {
		i = alGetError();

		if ( i != AL_NO_ERROR )
			return (const char *)alGetString(i);
	}

	return NULL;
}

//--------------------------------------------------------------------------
// ds_is_3d_buffer()
//
// Determine if a secondary buffer is a 3d secondary buffer.
//
int ds_is_3d_buffer(int sid)
{
	// OpenAL sources are always positional
	if ( sid >= 0 ) {
		return 1;
	}

	return 0;
}

//--------------------------------------------------------------------------
//  ds_build_vol_lookup()
//
//  Fills up the ds_vol_lookup[] tables that converts from a volume in the form
//  0.0 -> 1.0 to -10000 -> 0 (this is the DirectSound method, where units are
//  hundredths of decibls)
//
void ds_build_vol_lookup()
{
	int	i;
	float	vol;

	ds_vol_lookup[0] = -10000;
	for ( i = 1; i <= 100; i++ ) {
		vol = i / 100.0f;
		ds_vol_lookup[i] = fl2i( (log(vol) / log(2.0f)) * 1000.0f);
	}
}


//--------------------------------------------------------------------------
// ds_convert_volume()
//
// Takes volume between 0.0f and 1.0f and converts into
// DirectSound style volumes between -10000 and 0.
int ds_convert_volume(float volume)
{
	int index;

	index = fl2i(volume * 100.0f);
	if ( index > 100 )
		index = 100;
	if ( index < 0 )
		index = 0;

	return ds_vol_lookup[index];
}

//--------------------------------------------------------------------------
// ds_get_percentage_vol()
//
// Converts -10000 -> 0 range volume to 0 -> 1
float ds_get_percentage_vol(int ds_vol)
{
	double vol;
	vol = pow(2.0, ds_vol/1000.0);
	return (float)vol;
}

// ds_ds_to_al_gain()
//
// DirectSound volumes are hundredths of decibels; AL gain is linear.
static ALfloat ds_ds_to_al_gain(int ds_vol)
{
	if ( ds_vol == -10000 )
		return 0.0f;

	return powf(10.0f, (float)ds_vol / (-600.0f / log10f(.5f)));
}

// ---------------------------------------------------------------------------------------
// ds_parse_wave()
//
// Parse a wave file.
//
// parameters:		filename			=> file of sound to parse
//						dest				=> address of pointer of where to store raw sound data (output parm)
//						dest_size		=> number of bytes of sound data stored (output parm)
//						header			=> address of pointer to a WAVEFORMATEX struct (output parm)
//
// returns:			0					=> wave file successfully parsed
//						-1					=> error
//
//	NOTE: memory is malloced for the header and dest in this function.  It is the responsibility
//			of the caller to free this memory later.
//
int ds_parse_wave(char *filename, ubyte **dest, uint *dest_size, WAVEFORMATEX **header)
{
	CFILE				*fp;
	int				cbExtra = 0;
	unsigned int	tag, size, next_chunk;

	// the 'fmt ' chunk fields, read individually (the on-disk struct is
	// byte-packed; the in-memory WAVEFORMATEX is not)
	WORD	wFormatTag, nChannels, nBlockAlign, wBitsPerSample;
	DWORD	nSamplesPerSec, nAvgBytesPerSec;

	fp = cfopen( filename, "rb" );
	if ( fp == NULL )	{
		nprintf(("Error", "Couldn't open '%s'\n", filename ));
		return -1;
	}

	// Skip the "RIFF" tag and file size (8 bytes)
	// Skip the "WAVE" tag (4 bytes)
	cfseek( fp, 12, CF_SEEK_SET );

	// Now read RIFF tags until the end of file

	while(1)	{
		if ( cfread( &tag, sizeof(uint), 1, fp ) != 1 )
			break;

		if ( cfread( &size, sizeof(uint), 1, fp ) != 1 )
			break;

		next_chunk = cftell(fp) + size;

		switch( tag )	{
		case 0x20746d66:		// The 'fmt ' tag
			//nprintf(("Sound", "SOUND => size of fmt block: %d\n", size));
			wFormatTag		= cfread_ushort(fp);
			nChannels		= cfread_ushort(fp);
			nSamplesPerSec	= cfread_uint(fp);
			nAvgBytesPerSec	= cfread_uint(fp);
			nBlockAlign		= cfread_ushort(fp);
			wBitsPerSample	= cfread_ushort(fp);

			if ( wFormatTag != WAVE_FORMAT_PCM ) {
				cbExtra = cfread_short(fp);
			}

			// Allocate memory for WAVEFORMATEX structure + extra bytes
			if ( (*header = (WAVEFORMATEX *) malloc ( sizeof(WAVEFORMATEX)+cbExtra )) != NULL ){
				(*header)->wFormatTag		= wFormatTag;
				(*header)->nChannels		= nChannels;
				(*header)->nSamplesPerSec	= nSamplesPerSec;
				(*header)->nAvgBytesPerSec	= nAvgBytesPerSec;
				(*header)->nBlockAlign		= nBlockAlign;
				(*header)->wBitsPerSample	= wBitsPerSample;
				(*header)->cbSize			= (unsigned short)cbExtra;

				// Read those extra bytes, append to WAVEFORMATEX structure
				if (cbExtra != 0) {
					cfread( ((ubyte *)(*header) + sizeof(WAVEFORMATEX)), cbExtra, 1, fp);
				}
			}
			else {
				Assert(0);		// malloc failed
			}

			break;
		case 0x61746164:		// the 'data' tag
			*dest_size = size;
			(*dest) = (ubyte *)malloc(size);
			Assert( *dest != NULL );
			cfread( *dest, size, 1, fp );
			break;
		default:	// unknown, skip it
			break;
		}
		cfseek( fp, next_chunk, CF_SEEK_SET );
	}
	cfclose(fp);

	return 0;
}


// ---------------------------------------------------------------------------------------
// ds_get_sid()
//
//
int ds_get_sid()
{
	int i;

	for ( i = 0; i < MAX_DS_SOFTWARE_BUFFERS; i++ ) {
		if ( sound_buffers[i].buf_id == 0 )
		break;
	}

	if ( i == MAX_DS_SOFTWARE_BUFFERS )	{
		return -1;
	}

	return i;
}

// ---------------------------------------------------------------------------------------
// ds_get_hid()
//
//
int ds_get_hid()
{
	// no hardware buffers under OpenAL
	return -1;
}

// ---------------------------------------------------------------------------------------
// Load a secondary buffer with sound data.  The sounds data for game sounds
// are stored in AL buffers, and are bound to an AL source in the Channels[]
// array to be played.
//
//
// parameters:
//					 sid				  => pointer to software id for sound ( output parm)
//					 hid				  => pointer to hardware id for sound ( output parm)
//					 final_size		  => pointer to storage to receive uncompressed sound size (output parm)
//              header          => pointer to a WAVEFORMATEX structure
//					 si				  => sound_info structure, contains details on the sound format
//					 flags			  => buffer properties ( DS_HARDWARE , DS_3D )
//
// returns:     -1           => sound effect could not loaded into a secondary buffer
//               0           => sound effect successfully loaded into a secondary buffer
//
//
// NOTE: this function is slow.  Don't call this function from within gameplay.
//
int ds_load_buffer(int *sid, int *hid, int *final_size, void *header, sound_info *si, int flags)
{
	Assert( final_size != NULL );
	Assert( header != NULL );
	Assert( si != NULL );
	Assert( si->data != NULL );
	Assert( si->size > 0 );
	Assert( si->sample_rate > 0);
	Assert( si->bits > 0 );
	Assert( si->n_channels > 0 );
	Assert( si->n_block_align >= 0 );
	Assert( si->avg_bytes_per_sec > 0 );

	if (!ds_initialized) {
		return -1;
	}

	// All sounds are required to have a software buffer
	*sid = ds_get_sid();
	if ( *sid == -1 ) {
		nprintf(("Sound","SOUND ==> No more sound buffers available\n"));
		return -1;
	}

	if ( hid )
		*hid = -1;

	ALuint pi;
	OpenAL_ErrorCheck( alGenBuffers(1, &pi), return -1 );

	ALenum format;
	ALsizei size;
	ALint bits, bps;
	ALuint frequency;
	ALvoid *data = NULL;

	// the below two convert_ variables are only used when the wav format is not
	// PCM.  We must convert to PCM before handing the data to OpenAL.
	ubyte *convert_buffer = NULL;		// storage for converted wav file
	int	convert_len;					// num bytes of converted wav file
	uint	src_bytes_used;				// number of source bytes actually converted (should always be equal to original size)
	int	rc;
	WAVEFORMATEX *pwfx = (WAVEFORMATEX *)header;

	switch ( si->format ) {
		case WAVE_FORMAT_PCM:
			bits = si->bits;
			bps  = si->avg_bytes_per_sec;
			size = si->size;
			data = si->data;
			break;

		case WAVE_FORMAT_ADPCM:
			// this ADPCM decoder decodes to 16-bit only so keep that in mind
			nprintf(( "Sound", "SOUND ==> converting sound from ADPCM to PCM\n" ));
			rc = ACM_convert_ADPCM_to_PCM(pwfx, si->data, si->size, &convert_buffer, 0, &convert_len, &src_bytes_used, 16);
			if ( rc == -1 ) {
				return -1;
			}

			if (src_bytes_used != si->size) {
				Int3();	// ACM conversion failed?
				return -1;
			}

			bits = 16;
			bps  = (((si->n_channels * bits) / 8) * si->sample_rate);
			size = convert_len;
			data = convert_buffer;

			nprintf(( "Sound", "SOUND ==> Coverted sound from ADPCM to PCM successfully\n" ));
			break;

		default:
			nprintf(( "Sound", "Unsupported sound encoding\n" ));
			return -1;
	}

	// format is now PCM
	frequency = si->sample_rate;

	if (bits == 16) {
		if (si->n_channels == 2) {
			format = AL_FORMAT_STEREO16;
		} else if (si->n_channels == 1) {
			format = AL_FORMAT_MONO16;
		} else {
			return -1;
		}
	} else if (bits == 8) {
		if (si->n_channels == 2) {
			format = AL_FORMAT_STEREO8;
		} else if (si->n_channels == 1) {
			format = AL_FORMAT_MONO8;
		} else {
			return -1;
		}
	} else {
		return -1;
	}

	Snd_sram += size;
	*final_size = size;

	OpenAL_ErrorCheck( alBufferData(pi, format, data, size, frequency), return -1 );

	sound_buffers[*sid].buf_id = pi;
	sound_buffers[*sid].source_id = -1;
	sound_buffers[*sid].frequency = frequency;
	sound_buffers[*sid].bits_per_sample = bits;
	sound_buffers[*sid].nchannels = si->n_channels;
	sound_buffers[*sid].nseconds = size / bps;
	sound_buffers[*sid].nbytes = size;

	if ( convert_buffer )
		free( convert_buffer );

	return 0;
}

// ---------------------------------------------------------------------------------------
// ds_init_channels()
//
// init the Channels[] array
//
void ds_init_channels()
{
	int i, n;
	ALuint *sids;

	sids = (ALuint *) malloc( sizeof(ALuint) * MAX_CHANNELS );
	Assert( sids != NULL );

	// Try and generate up to MAX_CHANNELS worth of sources; if the
	// implementation runs out earlier, shrink MAX_CHANNELS to what we got.

	// clear the current error state before doing anything else
	n = alGetError();
	while ( n != AL_NO_ERROR ) {
		n = alGetError();
	}

	for ( i = 0; i < MAX_CHANNELS; i++ ) {
		alGenSources( 1, &sids[i] );
		n = alGetError();
		if ( n != AL_NO_ERROR )
			break;
	}

	if ( i != MAX_CHANNELS ) {
		nprintf(("Warning", "OpenAL: Restricting MAX_CHANNELS to %i (default: %i)\n", i, MAX_CHANNELS));
		MAX_CHANNELS = i;
	}

	// now delete them again so the game can make the real ones on demand
	for ( i = 0; i < MAX_CHANNELS; i++ ) {
		if ( (sids[i] != 0) && alIsSource(sids[i]) )
			OpenAL_ErrorPrint( alDeleteSources(1, &sids[i]) );
	}

	free( sids );

	Channels = (channel *) malloc( sizeof(channel) * MAX_CHANNELS );
	if (Channels == NULL) {
		Error(LOCATION, "Unable to allocate %d bytes for %d audio channels.", (int)(sizeof(channel) * MAX_CHANNELS), MAX_CHANNELS);
	}

	memset( Channels, 0, sizeof(channel) * MAX_CHANNELS );

	// init the channels
	for ( i = 0; i < MAX_CHANNELS; i++ ) {
		Channels[i].source_id = 0;
		Channels[i].buf_id = -1;
		Channels[i].sig = -1;
		Channels[i].snd_id = -1;
	}
}

// ---------------------------------------------------------------------------------------
// ds_init_buffers()
//
// init the sound_buffers[] array
//
void ds_init_buffers()
{
	int i;

	memset( sound_buffers, 0, sizeof(sound_buffers) );

	for ( i = 0; i < MAX_DS_SOFTWARE_BUFFERS; i++ ) {
		sound_buffers[i].buf_id = 0;
		sound_buffers[i].source_id = -1;
	}
}

// Fill in the waveformat struct with the primary buffer characteristics.
void ds_get_primary_format(WAVEFORMATEX *wfx)
{
	// the mixer format; only the (stubbed) capture/rtvoice paths look at this
	wfx->wFormatTag			= WAVE_FORMAT_PCM;
	wfx->nChannels			= 1;
	wfx->nSamplesPerSec		= 22050;
	wfx->wBitsPerSample		= 16;
	wfx->cbSize				= 0;
	wfx->nBlockAlign		= (unsigned short)(wfx->nChannels * wfx->wBitsPerSample / 8);
	wfx->nAvgBytesPerSec	= wfx->nBlockAlign * wfx->nSamplesPerSec;
}

// ---------------------------------------------------------------------------------------
// ds_init()
//
// returns:     -1           => init failed
//               0           => init success
int ds_init(int use_a3d, int use_eax)
{
	// NOTE: A3D and EAX are unused under OpenAL
	int attr[] = { ALC_FREQUENCY, 22050, ALC_SYNC, AL_FALSE, 0 };
	ALfloat list_orien[] = { 0.0f, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f };

	Ds_use_a3d = 0;
	Ds_use_eax = 0;
	Ds_use_ds3d = 0;

	mprintf(("Initializing OpenAL...\n"));

	// clear out all errors before moving on
	alcGetError(NULL);

	ds_sound_device = alcOpenDevice( NULL );

	if ( !ds_sound_device )
		goto AL_InitError;

	OpenAL_C_ErrorCheck( { ds_sound_context = alcCreateContext( ds_sound_device, attr ); }, goto AL_InitError );

	// set the new context as current
	OpenAL_C_ErrorCheck( alcMakeContextCurrent( ds_sound_context ), goto AL_InitError );

	mprintf(( "  OpenAL Vendor     : %s\n", alGetString( AL_VENDOR ) ));
	mprintf(( "  OpenAL Renderer   : %s\n", alGetString( AL_RENDERER ) ));
	mprintf(( "  OpenAL Version    : %s\n", alGetString( AL_VERSION ) ));

	// setup default listener position/orientation
	// this is needed for 2D pan
	OpenAL_ErrorPrint( alListener3f(AL_POSITION, 0.0, 0.0, 0.0) );
	OpenAL_ErrorPrint( alListenerfv(AL_ORIENTATION, list_orien) );

	ds_build_vol_lookup();
	ds_init_channels();
	ds_init_buffers();

	// clear out all errors before moving on
	alcGetError(ds_sound_device);
	alGetError();

	mprintf(("... OpenAL successfully initialized!\n"));

	ds_initialized = TRUE;

	return 0;


AL_InitError:
	alcMakeContextCurrent(NULL);

	if (ds_sound_context != NULL) {
		alcDestroyContext(ds_sound_context);
		ds_sound_context = NULL;
	}

	if (ds_sound_device != NULL) {
		alcCloseDevice(ds_sound_device);
		ds_sound_device = NULL;
	}

	return -1;
}

// ---------------------------------------------------------------------------------------
// get_DSERR_text()
//
// returns the text equivalent for the a DirectSound DSERR_ code
//
char *get_DSERR_text(int DSResult)
{
	// no DirectSound result codes anymore
	return (char *)"unknown";
}


// ---------------------------------------------------------------------------------------
// ds_close_channel()
//
// Free a single channel
//
void ds_close_channel(int i)
{
	if ( (Channels[i].source_id != 0) && alIsSource(Channels[i].source_id) ) {
		OpenAL_ErrorPrint( alSourceStop(Channels[i].source_id) );

		OpenAL_ErrorPrint( alDeleteSources(1, &Channels[i].source_id) );

		Channels[i].source_id = 0;
		Channels[i].buf_id = -1;
		Channels[i].sig = -1;
		Channels[i].snd_id = -1;
	}
}

// ---------------------------------------------------------------------------------------
// ds_close_all_channels()
//
// Free all the channel buffers
//
void ds_close_all_channels()
{
	int		i;

	for (i = 0; i < MAX_CHANNELS; i++)	{
		ds_close_channel(i);
	}
}

// ---------------------------------------------------------------------------------------
// ds_unload_buffer()
//
//
void ds_unload_buffer(int sid, int hid)
{
	if (sid != -1) {
		ALuint buf_id = sound_buffers[sid].buf_id;
		int channel_idx = sound_buffers[sid].source_id;

		if (channel_idx != -1)
			ds_close_channel(channel_idx);

		if ( (buf_id != 0) && alIsBuffer(buf_id) )
			OpenAL_ErrorPrint( alDeleteBuffers(1, &buf_id) );

		sound_buffers[sid].buf_id = 0;
		sound_buffers[sid].source_id = -1;
	}

	/* hid unused */
}

// ---------------------------------------------------------------------------------------
// ds_close_buffers()
//
// Free the sound buffers
//
void ds_close_buffers()
{
	int i;

	for (i = 0; i < MAX_DS_SOFTWARE_BUFFERS; i++) {
		ALuint buf_id = sound_buffers[i].buf_id;

		if ( (buf_id != 0) && alIsBuffer(buf_id) ) {
			OpenAL_ErrorPrint( alDeleteBuffers(1, &buf_id) );
		}

		sound_buffers[i].buf_id = 0;
		sound_buffers[i].source_id = -1;
	}
}

// ---------------------------------------------------------------------------------------
// ds_close()
//
// Close the sound system
//
void ds_close()
{
	if (!ds_initialized)
		return;

	ds_close_buffers();
	ds_close_all_channels();

	// free the Channels[] array, since it was dynamically allocated
	free(Channels);
	Channels = NULL;

	alcMakeContextCurrent(NULL);

	if (ds_sound_context != NULL) {
		alcDestroyContext(ds_sound_context);
		ds_sound_context = NULL;
	}

	if (ds_sound_device != NULL) {
		alcCloseDevice(ds_sound_device);
		ds_sound_device = NULL;
	}

	ds_initialized = FALSE;
}

// ---------------------------------------------------------------------------------------
// ds_get_free_channel()
//
// Find a free channel to play a sound on.  If no free channels exists, free up one based
// on volume levels.
//
//	input:		new_volume	=>		volume in DS units for sound to play at
//					snd_id		=>		which kind of sound to play
//					priority		=>		DS_MUST_PLAY
//											DS_LIMIT_ONE
//											DS_LIMIT_TWO
//											DS_LIMIT_THREE
//
//	returns:		channel number to play sound on
//					-1 if no channel could be found
//
// NOTE:	snd_id is needed since we limit the number of concurrent samples
//
//
int ds_get_free_channel(int new_volume, int snd_id, int priority)
{
	int				i, first_free_channel, limit;
	int				lowest_vol = 0, lowest_vol_index = -1;
	int				instance_count;	// number of instances of sound already playing
	int				lowest_instance_vol, lowest_instance_vol_index;
	channel			*chp;
	ALint				status;

	instance_count = 0;
	lowest_instance_vol = 99;
	lowest_instance_vol_index = -1;
	first_free_channel = -1;

	// Look for a channel to use to play this sample
	for ( i = 0; i < MAX_CHANNELS; i++ )	{
		chp = &Channels[i];

		if ( chp->source_id == 0 ) {
			if ( first_free_channel == -1 )
				first_free_channel = i;

			continue;
		}

		OpenAL_ErrorCheck( alGetSourcei(chp->source_id, AL_SOURCE_STATE, &status), continue );

		if ( status != AL_PLAYING ) {
			if ( first_free_channel == -1 )
				first_free_channel = i;

			ds_close_channel(i);

			continue;
		}
		else {
			if ( chp->snd_id == snd_id ) {
				instance_count++;
				if ( chp->vol < lowest_instance_vol && chp->looping == FALSE ) {
					lowest_instance_vol = chp->vol;
					lowest_instance_vol_index = i;
				}
			}

			if ( chp->vol < lowest_vol && chp->looping == FALSE ) {
				lowest_vol_index = i;
				lowest_vol = chp->vol;
			}
		}
	}

	// determine the limit of concurrent instances of this sound
	switch(priority) {
		case DS_MUST_PLAY:
			limit = 100;
			break;
		case DS_LIMIT_ONE:
			limit = 1;
			break;
		case DS_LIMIT_TWO:
			limit = 2;
			break;
		case DS_LIMIT_THREE:
			limit = 3;
			break;
		default:
			Int3();			// get Alan
			limit = 100;
			break;
	}

	// If we've exceeded the limit, then maybe stop the duplicate if it is lower volume
	if ( instance_count >= limit ) {
		// If there is a lower volume duplicate, stop it.... otherwise, don't play the sound
		if ( lowest_instance_vol_index >= 0 && (Channels[lowest_instance_vol_index].vol <= new_volume) ) {
			ds_close_channel(lowest_instance_vol_index);
			first_free_channel = lowest_instance_vol_index;
		} else {
			first_free_channel = -1;
		}
	} else {
		// there is no limit barrier to play the sound, so see if we've ran out of channels
		if ( first_free_channel == -1 ) {
			// stop the lowest volume instance to play our sound if priority demands it
			if ( lowest_vol_index != -1 && priority == DS_MUST_PLAY ) {
				// Check if the lowest volume playing is less than the volume of the requested sound.
				// If so, then we are going to trash the lowest volume sound.
				if ( Channels[lowest_vol_index].vol <= new_volume ) {
					ds_close_channel(lowest_vol_index);
					first_free_channel = lowest_vol_index;
				}
			}
		}
	}

	if ( (first_free_channel >= 0) && (Channels[first_free_channel].source_id == 0) )
		OpenAL_ErrorCheck( alGenSources(1, &Channels[first_free_channel].source_id), return -1 );

	return first_free_channel;
}

// Create a sound buffer, without locking any data in
int ds_create_buffer(int frequency, int bits_per_sample, int nchannels, int nseconds)
{
	ALuint i;
	int sid;

	if (!ds_initialized) {
		return -1;
	}

	sid = ds_get_sid();
	if ( sid == -1 ) {
		nprintf(("Sound","SOUND ==> No more OpenAL buffers available\n"));
		return -1;
	}

	OpenAL_ErrorCheck( alGenBuffers(1, &i), return -1 );

	sound_buffers[sid].buf_id = i;
	sound_buffers[sid].source_id = -1;
	sound_buffers[sid].frequency = frequency;
	sound_buffers[sid].bits_per_sample = bits_per_sample;
	sound_buffers[sid].nchannels = nchannels;
	sound_buffers[sid].nseconds = nseconds;
	sound_buffers[sid].nbytes = nseconds * (bits_per_sample / 8) * nchannels * frequency;

	return sid;
}

// Lock data into an existing buffer
int ds_lock_data(int sid, unsigned char *data, int size)
{
	Assert(sid >= 0);

	ALuint buf_id = sound_buffers[sid].buf_id;
	ALenum format;

	if (sound_buffers[sid].bits_per_sample == 16) {
		if (sound_buffers[sid].nchannels == 2) {
			format = AL_FORMAT_STEREO16;
		} else if (sound_buffers[sid].nchannels == 1) {
			format = AL_FORMAT_MONO16;
		} else {
			return -1;
		}
	} else if (sound_buffers[sid].bits_per_sample == 8) {
		if (sound_buffers[sid].nchannels == 2) {
			format = AL_FORMAT_STEREO8;
		} else if (sound_buffers[sid].nchannels == 1) {
			format = AL_FORMAT_MONO8;
		} else {
			return -1;
		}
	} else {
		return -1;
	}

	sound_buffers[sid].nbytes = size;

	OpenAL_ErrorCheck( alBufferData(buf_id, format, data, size, sound_buffers[sid].frequency), return -1 );

	return 0;
}

// Stop a buffer from playing directly
void ds_stop_easy(int sid)
{
	Assert(sid >= 0);

	int cid = sound_buffers[sid].source_id;

	if (cid != -1) {
		ALuint source_id = Channels[cid].source_id;

		if (source_id != 0)
			OpenAL_ErrorPrint( alSourceStop(source_id) );
	}
}

//	Play a sound without the usual baggage (used for playing back real-time voice)
//
// parameters:
//					sid			=> software id of sound
//					volume      => volume of sound effect in DirectSound units
int ds_play_easy(int sid, int volume)
{
	if (!ds_initialized)
		return -1;

	int ch_idx = ds_get_free_channel(volume, -1, DS_MUST_PLAY);

	if (ch_idx < 0)
		return -1;

	ALuint source_id = Channels[ch_idx].source_id;

	OpenAL_ErrorPrint( alSourceStop(source_id) );

	if (Channels[ch_idx].buf_id != sid) {
		ALuint buffer_id = sound_buffers[sid].buf_id;

		OpenAL_ErrorCheck( alSourcei(source_id, AL_BUFFER, buffer_id), return -1 );
	}

	Channels[ch_idx].buf_id = sid;

	OpenAL_ErrorPrint( alSourcef(source_id, AL_GAIN, ds_ds_to_al_gain(volume)) );

	OpenAL_ErrorPrint( alSourcei(source_id, AL_LOOPING, AL_FALSE) );

	OpenAL_ErrorPrint( alSourcePlay(source_id) );

	return 0;
}

// ---------------------------------------------------------------------------------------
// Play a secondary buffer.
//
//
// parameters:
//					sid			=> software id of sound
//					hid			=> hardware id of sound ( -1 if not in hardware )
//					snd_id		=>	what kind of sound this is
//					priority		=>		DS_MUST_PLAY
//											DS_LIMIT_ONE
//											DS_LIMIT_TWO
//											DS_LIMIT_THREE
//					volume      => volume of sound effect in DirectSound units
//					pan         => pan of sound in DirectSound units
//             looping     => whether the sound effect is looping or not
//
// returns:    -1          => sound effect could not be started
//              >=0        => sig for sound effect successfully started
//
int ds_play(int sid, int hid, int snd_id, int priority, int volume, int pan, int looping, bool is_voice_msg)
{
	int ch_idx;

	if (!ds_initialized)
		return -1;

	ch_idx = ds_get_free_channel(volume, snd_id, priority);

	if (ch_idx < 0) {
//		nprintf(( "Sound", "SOUND ==> Not playing sound requested at volume %.2f\n", ds_get_percentage_vol(volume) ));
		return -1;
	}

	if (Channels[ch_idx].source_id == 0)
		return -1;

	// set new position for pan or zero out if none
	ALfloat alpan = (float)pan / MAX_PAN;

	if ( alpan ) {
		OpenAL_ErrorPrint( alSource3f(Channels[ch_idx].source_id, AL_POSITION, alpan, 0.0, 1.0) );
	} else {
		OpenAL_ErrorPrint( alSource3f(Channels[ch_idx].source_id, AL_POSITION, 0.0, 0.0, 0.0) );
	}

	OpenAL_ErrorPrint( alSource3f(Channels[ch_idx].source_id, AL_VELOCITY, 0.0, 0.0, 0.0) );

	OpenAL_ErrorPrint( alSourcef(Channels[ch_idx].source_id, AL_PITCH, 1.0) );

	OpenAL_ErrorPrint( alSourcef(Channels[ch_idx].source_id, AL_GAIN, ds_ds_to_al_gain(volume)) );

	ALint status;
	OpenAL_ErrorCheck( alGetSourcei(Channels[ch_idx].source_id, AL_SOURCE_STATE, &status), return -1 );

	if (status == AL_PLAYING)
		OpenAL_ErrorPrint( alSourceStop(Channels[ch_idx].source_id) );

	OpenAL_ErrorCheck( alSourcei(Channels[ch_idx].source_id, AL_BUFFER, sound_buffers[sid].buf_id), return -1 );

	OpenAL_ErrorPrint( alSourcei(Channels[ch_idx].source_id, AL_SOURCE_RELATIVE, AL_FALSE) );

	OpenAL_ErrorPrint( alSourcei(Channels[ch_idx].source_id, AL_LOOPING, (looping) ? AL_TRUE : AL_FALSE) );

	OpenAL_ErrorPrint( alSourcePlay(Channels[ch_idx].source_id) );

	sound_buffers[sid].source_id = ch_idx;

	Channels[ch_idx].buf_id = sid;
	Channels[ch_idx].snd_id = snd_id;
	Channels[ch_idx].sig = channel_next_sig++;
	Channels[ch_idx].last_position = 0;
	Channels[ch_idx].is_voice_msg = is_voice_msg;
	Channels[ch_idx].vol = volume;
	Channels[ch_idx].looping = looping;
	Channels[ch_idx].priority = priority;

	if (channel_next_sig < 0)
		channel_next_sig = 1;

	return Channels[ch_idx].sig;
}


// ---------------------------------------------------------------------------------------
// ds_get_channel()
//
// Return the channel number that is playing the sound identified by sig.  If that sound is
// not playing, return -1.
//
int ds_get_channel(int sig)
{
	int i;

	if (!ds_initialized)
		return -1;

	for ( i = 0; i < MAX_CHANNELS; i++ ) {
		if ( Channels[i].source_id && (Channels[i].sig == sig) ) {
			if ( ds_is_channel_playing(i) == TRUE ) {
				return i;
			}
		}
	}

	return -1;
}

// ---------------------------------------------------------------------------------------
// ds_is_channel_playing()
//
//
int ds_is_channel_playing(int channel)
{
	if ( Channels[channel].source_id != 0 ) {
		ALint status;

		OpenAL_ErrorPrint( alGetSourcei(Channels[channel].source_id, AL_SOURCE_STATE, &status) );

		return (status == AL_PLAYING);
	}

	return 0;
}

// ---------------------------------------------------------------------------------------
// ds_stop_channel()
//
//
void ds_stop_channel(int channel)
{
	if ( Channels[channel].source_id != 0 ) {
		OpenAL_ErrorPrint( alSourceStop(Channels[channel].source_id) );
	}
}

// ---------------------------------------------------------------------------------------
// ds_stop_channel_all()
//
//
void ds_stop_channel_all()
{
	int i;

	if (!ds_initialized)
		return;

	for ( i=0; i<MAX_CHANNELS; i++ )	{
		if ( Channels[i].source_id != 0 ) {
			OpenAL_ErrorPrint( alSourceStop(Channels[i].source_id) );
		}
	}
}

// ---------------------------------------------------------------------------------------
// ds_set_volume()
//
//	Set the volume for a channel.  The volume is expected to be in DirectSound units
//
//	If the sound is a 3D sound buffer, this is like re-establishing the maximum
// volume.
//
void ds_set_volume( int channel, int vol )
{
	ALuint source_id = Channels[channel].source_id;

	if (source_id != 0) {
		OpenAL_ErrorPrint( alSourcef(source_id, AL_GAIN, ds_ds_to_al_gain(vol)) );
	}
}

// ---------------------------------------------------------------------------------------
// ds_set_pan()
//
//	Set the pan for a channel.  The pan is expected to be in DirectSound units
//
void ds_set_pan( int channel, int pan )
{
	ALint state;

	OpenAL_ErrorCheck( alGetSourcei(Channels[channel].source_id, AL_SOURCE_STATE, &state), return );

	if (state == AL_PLAYING) {
		ALfloat alpan = (pan != 0) ? ((float)pan / MAX_PAN) : 0.0f;
		OpenAL_ErrorPrint( alSource3f(Channels[channel].source_id, AL_POSITION, alpan, 0.0, 1.0) );
	}
}

// ---------------------------------------------------------------------------------------
// ds_get_pitch()
//
//	Get the pitch of a channel
//
int ds_get_pitch(int channel)
{
	ALint status;
	ALfloat alpitch = 0;
	int pitch;

	OpenAL_ErrorCheck( alGetSourcei(Channels[channel].source_id, AL_SOURCE_STATE, &status), return -1 );

	if (status == AL_PLAYING)
		OpenAL_ErrorPrint( alGetSourcef(Channels[channel].source_id, AL_PITCH, &alpitch) );

	// convert OpenAL values to DirectSound values and return
	pitch = fl2i( pow(10.0, (alpitch + 2.0)) );

	return pitch;
}

// ---------------------------------------------------------------------------------------
// ds_set_pitch()
//
//	Set the pitch of a channel
//
void ds_set_pitch(int channel, int pitch)
{
	ALint status;

	if ( pitch < MIN_PITCH )
		pitch = MIN_PITCH;

	if ( pitch > MAX_PITCH )
		pitch = MAX_PITCH;

	OpenAL_ErrorCheck( alGetSourcei(Channels[channel].source_id, AL_SOURCE_STATE, &status), return );

	if (status == AL_PLAYING) {
		ALfloat alpitch = log10f((float)pitch) - 2.0f;
		OpenAL_ErrorPrint( alSourcef(Channels[channel].source_id, AL_PITCH, alpitch) );
	}
}

// ---------------------------------------------------------------------------------------
// ds_chg_loop_status()
//
//
void ds_chg_loop_status(int channel, int loop)
{
	ALuint source_id = Channels[channel].source_id;

	if (source_id != 0) {
		OpenAL_ErrorPrint( alSourcei(source_id, AL_LOOPING, loop ? AL_TRUE : AL_FALSE) );
	}
}

// ---------------------------------------------------------------------------------------
// ds3d_play()
//
// Starts a ds3d sound playing
//
//	input:
//
//					sid				=>	software id for sound to play
//					hid				=>	hardware id for sound to play (-1 if not in hardware)
//					snd_id			=> identifies what type of sound is playing
//					pos				=>	world pos of sound
//					vel				=>	velocity of object emitting sound
//					min				=>	distance at which sound doesn't get any louder
//					max				=>	distance at which sound becomes inaudible
//					looping			=>	boolean, whether to loop the sound or not
//					max_volume		=>	volume (-10000 to 0) for 3d sound at maximum
//					estimated_vol	=>	manual estimated volume
//					priority		=>		DS_MUST_PLAY
//											DS_LIMIT_ONE
//											DS_LIMIT_TWO
//											DS_LIMIT_THREE
//
//	returns:			0				=> sound started successfully
//						-1				=> sound could not be played
//
int ds3d_play(int sid, int hid, int snd_id, vector *pos, vector *vel, int min, int max, int looping, int max_volume, int estimated_vol, int priority )
{
	int ch_idx;
	ALint status;

	if (!ds_initialized)
		return -1;

	ch_idx = ds_get_free_channel(estimated_vol, snd_id, priority);

	if (ch_idx < 0) {
		return -1;
	}

	if (Channels[ch_idx].source_id == 0)
		return -1;

	// reset pitch value since it could have been changed for this source
	OpenAL_ErrorPrint( alSourcef(Channels[ch_idx].source_id, AL_PITCH, 1.0) );

	// set up 3D sound data here
	ds3d_update_buffer(ch_idx, i2fl(min), i2fl(max), pos, vel);

	Channels[ch_idx].vol = estimated_vol;
	Channels[ch_idx].looping = looping;
	Channels[ch_idx].priority = priority;

	// set volume
	OpenAL_ErrorPrint( alSourcef(Channels[ch_idx].source_id, AL_GAIN, ds_ds_to_al_gain(estimated_vol)) );

	// set maximum "inner cone" volume
	OpenAL_ErrorPrint( alSourcef(Channels[ch_idx].source_id, AL_MAX_GAIN, ds_ds_to_al_gain(max_volume)) );

	OpenAL_ErrorCheck( alGetSourcei(Channels[ch_idx].source_id, AL_SOURCE_STATE, &status), return -1 );

	if (status == AL_PLAYING)
		OpenAL_ErrorPrint( alSourceStop(Channels[ch_idx].source_id) );

	OpenAL_ErrorCheck( alSourcei(Channels[ch_idx].source_id, AL_BUFFER, sound_buffers[sid].buf_id), return -1 );

	OpenAL_ErrorPrint( alSourcei(Channels[ch_idx].source_id, AL_LOOPING, (looping) ? AL_TRUE : AL_FALSE) );

	OpenAL_ErrorPrint( alSourcei(Channels[ch_idx].source_id, AL_SOURCE_RELATIVE, AL_TRUE) );

	OpenAL_ErrorPrint( alSourcePlay(Channels[ch_idx].source_id) );

	sound_buffers[sid].source_id = ch_idx;

	Channels[ch_idx].buf_id = sid;
	Channels[ch_idx].snd_id = snd_id;
	Channels[ch_idx].sig = channel_next_sig++;
	Channels[ch_idx].last_position = 0;

	if (channel_next_sig < 0)
		channel_next_sig = 1;

	return Channels[ch_idx].sig;
}

void ds_set_position(int channel, DWORD offset)
{
	OpenAL_ErrorPrint( alSourcei(Channels[channel].source_id, AL_BYTE_OFFSET, offset) );
}

DWORD ds_get_play_position(int channel)
{
	ALint pos = 0;

	if (Channels[channel].buf_id == -1)
		return 0;

	OpenAL_ErrorCheck( alGetSourcei(Channels[channel].source_id, AL_BYTE_OFFSET, &pos), return 0 );

	if ( pos < 0 )
		pos = 0;

	return pos;
}

DWORD ds_get_write_position(int channel)
{
	// no write cursor without a streaming DirectSound buffer
	return 0;
}

int ds_get_channel_size(int channel)
{
	int buf_id = Channels[channel].buf_id;

	if (buf_id != -1) {
		return sound_buffers[buf_id].nbytes;
	}

	return 0;
}

// Returns the number of channels that are actually playing
int ds_get_number_channels()
{
	int i,n;

	if (!ds_initialized) {
		return 0;
	}

	n = 0;
	for ( i = 0; i < MAX_CHANNELS; i++ ) {
		if ( Channels[i].source_id ) {
			if ( ds_is_channel_playing(i) == TRUE ) {
				n++;
			}
		}
	}

	return n;
}

// retreive raw data from a sound buffer
int ds_get_data(int sid, char *data)
{
	// OpenAL buffers cannot be read back
	return -1;
}

// return the size of the raw sound data
int ds_get_size(int sid, int *size)
{
	Assert(sid >= 0);

	if ( sound_buffers[sid].buf_id == 0 )
		return -1;

	if ( size )
		*size = sound_buffers[sid].nbytes;

	return 0;
}

int ds_using_ds3d()
{
	return Ds_use_ds3d;
}

bool ds_using_a3d()
{
	return false;
}

// Return the primary buffer interface.  Retail handed out raw DirectSound
// COM pointers here; nothing exists to hand out anymore.
//
unsigned int ds_get_primary_buffer_interface()
{
	return 0;
}

// Return the DirectSound Interface.
//
unsigned int ds_get_dsound_interface()
{
	return 0;
}

unsigned int ds_get_property_set_interface()
{
	return 0;
}

// --------------------
//
// EAX Functions below
//
// --------------------

// Set the master volume for the reverb added to all sound sources.
//
// volume: volume, range from 0 to 1.0
//
// returns: 0 if the volume is set successfully, otherwise return -1
//
int ds_eax_set_volume(float volume)
{
	return -1;
}

// Set the decay time for the EAX environment (ie all sound sources)
//
// seconds: decay time in seconds
//
// returns: 0 if decay time is successfully set, otherwise return -1
//
int ds_eax_set_decay_time(float seconds)
{
	return -1;
}

// Set the damping value for the EAX environment (ie all sound sources)
//
// damp: damp value from 0 to 2.0
//
// returns: 0 if the damp value is successfully set, otherwise return -1
//
int ds_eax_set_damping(float damp)
{
	return -1;
}

// Set up the environment type for all sound sources.
//
// envid: value from the EAX_ENVIRONMENT_* enumeration in ds_eax.h
//
// returns: 0 if the environment is set successfully, otherwise return -1
//
int ds_eax_set_environment(unsigned long envid)
{
	return -1;
}

// Set up a predefined environment for EAX
//
// envid: value from teh EAX_ENVIRONMENT_* enumeration
//
// returns: 0 if successful, otherwise return -1
//
int ds_eax_set_preset(unsigned long envid)
{
	return -1;
}


// Set up all the parameters for an environment
//
// id: value from teh EAX_ENVIRONMENT_* enumeration
// volume: volume for the environment (0 to 1.0)
// damping: damp value for the environment (0 to 2.0)
// decay: decay time in seconds (0.1 to 20.0)
//
// returns: 0 if successful, otherwise return -1
//
int ds_eax_set_all(unsigned long id, float volume, float damping, float decay)
{
	return -1;
}

// Get up the parameters for the current environment
//
// er: (output) hold environment parameters
//
// returns: 0 if successful, otherwise return -1
//
int ds_eax_get_all(EAX_REVERBPROPERTIES *er)
{
	return -1;
}

// Close down EAX, freeing any allocated resources
//
void ds_eax_close()
{
}

// Initialize EAX
//
// returns: 0 if initialization is successful, otherwise return -1
//
int ds_eax_init()
{
	return -1;
}

int ds_eax_is_inited()
{
	return 0;
}

// Called once per game frame to make sure voice messages aren't looping
//
void ds_do_frame()
{
	if (!ds_initialized)
		return;

	int i;
	channel *cp = NULL;

	for (i = 0; i < MAX_CHANNELS; i++) {
		cp = &Channels[i];

		if (cp->is_voice_msg == true) {
			if( cp->source_id == 0 ) {
				continue;
			}

			DWORD current_position = ds_get_play_position(i);
			if (current_position != 0) {
				if (current_position < cp->last_position) {
					ds_close_channel(i);
				} else {
					cp->last_position = current_position;
				}
			}
		}
	}
}
