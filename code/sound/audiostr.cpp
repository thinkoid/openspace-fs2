/*
 * Copyright (C) Volition, Inc. 1999.  All rights reserved.
 *
 * All source code herein is the property of Volition, Inc. You may not sell
 * or otherwise commercially exploit the source or things you created based on the
 * source.
 *
*/

#include <stdio.h>
#include <string.h>
#include <math.h>

#include <SDL.h>
#include <AL/al.h>

#include "pstypes.h"

#include "audiostr.h"
#include "cfile.h"		// needed for cf_find_file_location
#include "timer.h"
#include "sound.h"		/* for Snd_sram */
#include "acm.h"
#include "ds.h"

// Constants
#ifndef SUCCESS
#define SUCCESS TRUE        // Error returns for all member functions
#define FAILURE FALSE
#endif // SUCCESS

typedef int (*TIMERCALLBACK)(void *);

#define MAX_STREAM_BUFFERS 4

#define BIGBUF_SIZE					180000			// This can be reduced to 88200 once we don't use any stereo
//#define BIGBUF_SIZE					88300			// This can be reduced to 88200 once we don't use any stereo
unsigned char *Wavedata_load_buffer = NULL;		// buffer used for cueing audiostreams
unsigned char *Wavedata_service_buffer = NULL;	// buffer used for servicing audiostreams

// The audiostream service timer fires on the SDL timer thread, so the
// critical sections here are real SDL mutexes (SDL mutexes are recursive,
// matching the Win32 CRITICAL_SECTION semantics the code was written for).
SDL_mutex *Global_service_lock = NULL;

#define COMPRESSED_BUFFER_SIZE	88300
unsigned char *Compressed_buffer = NULL;				// Used to load in compressed data during a cueing interval
unsigned char *Compressed_service_buffer = NULL;	// Used to read in compressed data during a service interval

#define AS_HIGHEST_MAX				999999999	// max uncompressed filesize supported is 999 meg

// the on-disk WAVEFORMATEX is 18 bytes; the ds.h struct pads to 20.  The ACM
// decoder parses the fmt chunk verbatim, so the ADPCM extra bytes must sit
// right after the 18 header bytes (matching acm.cpp)
#define WAVEFORMATEX_DISK_SIZE	18

// check for an OpenAL error from the preceding call, print it out.
// Returns 1 if an error was raised, 0 if all is well.
static int openal_error(const char *where)
{
	ALenum err = alGetError();

	if ( err != AL_NO_ERROR ) {
		nprintf(("SOUND", "SOUND ==> OpenAL error in %s: %s\n", where, (const char *)alGetString(err)));
		return 1;
	}

	return 0;
}

// wave chunk reading helpers.  The RIFF fields are read one by one, since
// the in-memory WAVEFORMATEX layout doesn't match the packed on-disk layout.
static int audiostr_read_word(FILE *fp, WORD *i)
{
	if ( fread( i, 1, sizeof(WORD), fp ) != sizeof(WORD) )
		return 0;

	*i = INTEL_SHORT(*i);
	return 1;
}

static int audiostr_read_dword(FILE *fp, DWORD *i)
{
	if ( fread( i, 1, sizeof(DWORD), fp ) != sizeof(DWORD) )
		return 0;

	*i = INTEL_INT(*i);
	return 1;
}

static int audiostr_read_uint(FILE *fp, uint *i)
{
	if ( fread( i, 1, sizeof(uint), fp ) != sizeof(uint) )
		return 0;

	*i = INTEL_INT(*i);
	return 1;
}

// Classes

// Timer
//
// Wrapper class for the SDL timer services. Provides
// periodic events. User must supply callback.
//

class Timer
{
public:
    void constructor(void);
    void destructor(void);
    int Create (uint nPeriod, uint nRes, void *dwUser, TIMERCALLBACK pfnCallback);
protected:
    static Uint32 TimeProc(Uint32 interval, void *param);
    TIMERCALLBACK m_pfnCallback;
    void *m_dwUser;
    uint m_nPeriod;
    uint m_nRes;
    SDL_TimerID m_nIDTimer;
};


// Class

// WaveFile
//
// WAV file class (read-only).
//
// Public Methods:
//
// Public Data:
//
//

class WaveFile
{
public:
	void Init(void);
	void Close(void);
	int Open (char *pszFilename);
	int Cue (void);
	int	Read (ubyte *pbDest, uint cbSize, int service=1);
	uint GetNumBytesRemaining (void) { return (m_nDataSize - m_nBytesPlayed); }
	uint GetUncompressedAvgDataRate (void) { return (m_nUncompressedAvgDataRate); }
	uint GetDataSize (void) { return (m_nDataSize); }
	uint GetNumBytesPlayed (void) { return (m_nBytesPlayed); }
	ubyte GetSilenceData (void);
	WAVEFORMATEX m_wfmt;					// format of wave file used by the sound system
	WAVEFORMATEX * m_pwfmt_original;	// foramt of wave file from actual wave source
	uint m_total_uncompressed_bytes_read;
	uint m_max_uncompressed_bytes_to_read;
	uint	m_bits_per_sample_uncompressed;
	ALenum m_ALformat;						// OpenAL buffer format matching m_wfmt

protected:
	uint m_data_offset;						// absolute file offset to actual wave data
	int  m_data_bytes_left;
	FILE	*cfp;

	uint m_wave_format;						// format of wave source (ie WAVE_FORMAT_PCM, WAVE_FORMAT_ADPCM)
	uint m_nBlockAlign;						// wave data block alignment spec
	uint m_nUncompressedAvgDataRate;		// average wave data rate
	uint m_nDataSize;							// size of data chunk
	uint m_nBytesPlayed;						// offset into data chunk
	int m_abort_next_read;

	void			*m_hStream;
	int				m_hStream_open;
	WAVEFORMATEX	m_wfxDest;
	char			m_wFilename[MAX_FILENAME_LEN];
};


// AudioStream
//
// Audio stream interface class for playing WAV files using OpenAL
// buffer queueing on a dedicated source.
//
// Public Methods:
//
// Public Data:
//

// status
#define ASF_FREE	0
#define ASF_USED	1

class AudioStream
{
public:
	AudioStream (void);
	~AudioStream (void);
	int Create (char *pszFilename);
	int Destroy (void);
	void Play (long volume, int looping);
	int Is_Playing(){ return(m_fPlaying); }
	int Is_Paused(){ return(m_bIsPaused); }
	int Is_Past_Limit() { return m_bPastLimit; }
	void Stop (int paused=0);
	void Stop_and_Rewind (void);
	void Fade_and_Destroy (void);
	void Fade_and_Stop(void);
	void	Set_Volume(long vol);
	long	Get_Volume();
	void	Init_Data();
	void	Set_Byte_Cutoff(unsigned int num_bytes_cutoff);
	void  Set_Default_Volume(long converted_volume) { m_lDefaultVolume = converted_volume; }
	long	Get_Default_Volume() { return m_lDefaultVolume; }
	unsigned int Get_Bytes_Committed(void);
	int	Is_looping() { return m_bLooping; }
	int	status;
	int	type;
	uint m_bits_per_sample_uncompressed;

protected:
	void Cue (void);
	int WriteWaveData (uint cbSize, uint* num_bytes_written,int service=1);
	uint GetMaxWriteSize (void);
	int ServiceBuffer (void);
	static int TimerCallback (void *dwUser);

	ALuint m_source_id;							// name of the OpenAL source
	ALuint m_buffer_ids[MAX_STREAM_BUFFERS];	// names of the queued buffers
	int m_play_buffer_id;						// round-robin index of next buffer to fill

	WaveFile * m_pwavefile;        // ptr to WaveFile object
	Timer m_timer;              // ptr to Timer object
	int m_fCued;                  // semaphore (stream cued)
	int m_fPlaying;               // semaphore (stream playing)
	long m_lInService;             // reentrancy semaphore
	uint m_cbBufOffset;            // last write position
	uint m_nBufLength;             // length of sound buffer in msec
	uint m_cbBufSize;              // size of sound buffer in bytes
	uint m_nBufService;            // service interval in msec
	uint m_nTimeStarted;           // time (in system time) playback started

	int	m_bLooping;						// whether or not to loop playback
	int	m_bFade;							// fade out music
	int	m_bDestroy_when_faded;
	long  m_lVolume;						// volume of stream ( 0 -> -10 000 )
	long	m_lCutoffVolume;
	int  m_bIsPaused;					// stream is stopped, but not rewinded
	uint  m_bReadingDone;				// no more bytes to be read from disk, still have remaining buffer to play
	uint	m_fade_timer_id;				// timestamp so we know when to start fade
	uint	m_finished_id;					// timestamp so we know when we've played #bytes required
	int	m_bPastLimit;					// flag to show we've played past the number of bytes requred
	long	m_lDefaultVolume;

	SDL_mutex *write_lock;
};


//
// AudioStream class implementation
//
////////////////////////////////////////////////////////////

// The following constants are the defaults for our streaming buffer operation.
const uint DefBufferLength          = 2000; // default buffer length in msec
const uint DefBufferServiceInterval = 250;  // default buffer service interval in msec

// Constructor
AudioStream::AudioStream (void)
{
	write_lock = SDL_CreateMutex();
}


// Destructor
AudioStream::~AudioStream (void)
{
	SDL_DestroyMutex( write_lock );
}


void AudioStream::Init_Data ()
{
	m_bLooping = 0;
	m_bFade = FALSE;
	m_fade_timer_id = 0;
	m_finished_id = 0;
	m_bPastLimit = FALSE;

	m_bDestroy_when_faded = FALSE;
	m_lVolume = 0;
	m_lCutoffVolume = -10000;
	m_bIsPaused = FALSE;
	m_bReadingDone = FALSE;

	m_pwavefile = NULL;
	m_fPlaying = m_fCued = FALSE;
	m_lInService = FALSE;
	m_cbBufOffset = 0;
	m_nBufLength = DefBufferLength;
	m_cbBufSize = 0;
	m_nBufService = DefBufferServiceInterval;
	m_nTimeStarted = 0;

	memset(m_buffer_ids, 0, sizeof(m_buffer_ids));
	m_source_id = 0;
	m_play_buffer_id = 0;
}

// Create
int AudioStream::Create (char *pszFilename)
{
	int fRtn = SUCCESS;    // assume success

	Assert(pszFilename);

	Init_Data();

	if (pszFilename) {
		// Create a new WaveFile object

		m_pwavefile = (WaveFile *)malloc(sizeof(WaveFile));
		Assert(m_pwavefile);

		if (m_pwavefile) {
			// Call constructor
			m_pwavefile->Init();
			// Open given file
			m_pwavefile->m_bits_per_sample_uncompressed = m_bits_per_sample_uncompressed;
			if (m_pwavefile->Open (pszFilename)) {
				// Calculate sound buffer size in bytes
				// Buffer size is average data rate times length of buffer
				// No need for buffer to be larger than wave data though
				m_cbBufSize = (m_pwavefile->GetUncompressedAvgDataRate () * m_nBufLength) / 1000;
				// cut it down by the number of buffers we rotate with to maintain some measure of sane memory usage
				m_cbBufSize /= MAX_STREAM_BUFFERS;

				// ??? there tends to be static in the audio if m_cbBufSize equals the samples per second, so make it unequal
				if ( (m_cbBufSize == m_pwavefile->m_wfmt.nSamplesPerSec) || (m_cbBufSize == 11025) || (m_cbBufSize == 22050) )
					m_cbBufSize = (uint)((float)m_cbBufSize * 1.3f);

				// if the requested buffer size is too big then cap it
				m_cbBufSize = (m_cbBufSize > BIGBUF_SIZE) ? BIGBUF_SIZE : m_cbBufSize;

				nprintf(("SOUND", "SOUND => Stream buffer created using %d bytes\n", m_cbBufSize));

				// Create the OpenAL source and rotating buffer set
				alGetError();
				alGenSources(1, &m_source_id);
				if ( openal_error("alGenSources") ) {
					fRtn = FAILURE;
					goto ErrorExit;
				}

				alGenBuffers(MAX_STREAM_BUFFERS, m_buffer_ids);
				if ( openal_error("alGenBuffers") ) {
					fRtn = FAILURE;
					goto ErrorExit;
				}

				// streams are always at full volume, straight ahead
				alSourcef(m_source_id, AL_ROLLOFF_FACTOR, 0);
				alSourcei(m_source_id, AL_SOURCE_RELATIVE, AL_TRUE);

				ALfloat posv[] = { 0, 0, 0 };
				alSourcefv(m_source_id, AL_POSITION, posv);

				alSourcef(m_source_id, AL_GAIN, 1);
				openal_error("source setup");

				// Cue for playback
				Cue ();
				Snd_sram += (m_cbBufSize * MAX_STREAM_BUFFERS);
			}
			else {
				// Error opening file
				nprintf(("SOUND", "SOUND => Failed to open wave file: %s\n\r", pszFilename));
				fRtn = FAILURE;
			}
		}
		else {
			// Error, unable to create WaveFile object
			nprintf(("Sound", "SOUND => Failed to create WaveFile object %s\n\r", pszFilename));
			fRtn = FAILURE;
		}
	}
	else {
		// Error, passed invalid parms
		fRtn = FAILURE;
	}

ErrorExit:
	if ( (fRtn == FAILURE) && (m_pwavefile) ) {
		if ( m_source_id ) {
			alDeleteSources(1, &m_source_id);
			m_source_id = 0;
		}

		m_pwavefile->Close();
		free(m_pwavefile);
		m_pwavefile = NULL;
	}

	return (fRtn);
}


// Destroy
int AudioStream::Destroy (void)
{
	int fRtn = SUCCESS;

	SDL_LockMutex(write_lock);

	// Stop playback
	Stop ();

	// Release sound source and buffers
	if ( m_source_id ) {
		alDeleteSources(1, &m_source_id);
		m_source_id = 0;

		for (int i = 0; i < MAX_STREAM_BUFFERS; i++) {
			// make sure that the buffer is real before trying to delete, it could crash for some otherwise
			if ( (m_buffer_ids[i] != 0) && alIsBuffer(m_buffer_ids[i]) ) {
				alDeleteBuffers(1, &m_buffer_ids[i]);
			}
			m_buffer_ids[i] = 0;
		}

		Snd_sram -= (m_cbBufSize * MAX_STREAM_BUFFERS);
	}

	// Delete WaveFile object
	if (m_pwavefile) {
		m_pwavefile->Close();
		free(m_pwavefile);
		m_pwavefile = NULL;
	}

	status = ASF_FREE;

	SDL_UnlockMutex(write_lock);

	return fRtn;
}

// WriteWaveData
//
// Writes wave data to a queued OpenAL buffer. This is a helper method used
// by Create and ServiceBuffer; it's not exposed to users of the AudioStream
// class.
int AudioStream::WriteWaveData (uint size, uint *num_bytes_written, int service)
{
	int fRtn = SUCCESS;
	unsigned char	*uncompressed_wave_data;

	*num_bytes_written = 0;

	if ( size == 0 || m_bReadingDone ) {
		return fRtn;
	}

	if ( (m_buffer_ids[0] == 0) || !m_pwavefile ) {
		return fRtn;
	}

	if ( service ) {
		SDL_LockMutex(Global_service_lock);
	}

	if ( service ) {
		uncompressed_wave_data = Wavedata_service_buffer;
	} else {
		uncompressed_wave_data = Wavedata_load_buffer;
	}

	int num_bytes_read = 0;

	num_bytes_read = m_pwavefile->Read(uncompressed_wave_data, m_cbBufSize, service);
	if ( num_bytes_read == -1 ) {
		// means nothing left to read!
		num_bytes_read = 0;
		m_bReadingDone = 1;
	}

	if ( num_bytes_read > 0 ) {
	//	nprintf(("SOUND", "SOUND ==> Queueing %d bytes of Data\n", num_bytes_read));

		// unqueue and recycle any processed buffers
		ALint p = 0;
		ALuint bid[MAX_STREAM_BUFFERS];

		alGetSourcei(m_source_id, AL_BUFFERS_PROCESSED, &p);

		if ( p > 0 ) {
			alSourceUnqueueBuffers(m_source_id, p, bid);
		}

		alGetError();
		alBufferData(m_buffer_ids[m_play_buffer_id], m_pwavefile->m_ALformat, uncompressed_wave_data, num_bytes_read, m_pwavefile->m_wfmt.nSamplesPerSec);
		if ( openal_error("alBufferData") ) {
			fRtn = FAILURE;
			goto ErrorExit;
		}

		alSourceQueueBuffers(m_source_id, 1, &m_buffer_ids[m_play_buffer_id]);
		if ( openal_error("alSourceQueueBuffers") ) {
			fRtn = FAILURE;
			goto ErrorExit;
		}

		m_play_buffer_id++;
		if (m_play_buffer_id >= MAX_STREAM_BUFFERS)
			m_play_buffer_id = 0;

		*num_bytes_written = num_bytes_read;
	}

ErrorExit:

	if ( service ) {
		SDL_UnlockMutex(Global_service_lock);
	}

	return (fRtn);
}


// GetMaxWriteSize
//
// Helper function to calculate max size of sound buffer write operation, i.e. how much
// free space there is in the buffer queue.
uint AudioStream::GetMaxWriteSize (void)
{
	uint dwMaxSize = m_cbBufSize;
	ALint n = 0, q = 0;

	alGetError();

	alGetSourcei(m_source_id, AL_BUFFERS_PROCESSED, &n);
	if ( openal_error("AL_BUFFERS_PROCESSED") )
		return 0;

	alGetSourcei(m_source_id, AL_BUFFERS_QUEUED, &q);
	if ( openal_error("AL_BUFFERS_QUEUED") )
		return 0;

	if (!n && (q >= MAX_STREAM_BUFFERS))	// all buffers queued
		dwMaxSize = 0;

//	nprintf(("Alan","Max write size: %d\n", dwMaxSize));
	return (dwMaxSize);
}


// ServiceBuffer
//
// Routine to service buffer requests initiated by periodic timer.
//
// Returns TRUE if buffer serviced normally; otherwise returns FALSE.
#define FADE_VOLUME_INTERVAL	 	 					400		// 100 == 1db
#define VOLUME_ATTENUATION_BEFORE_CUTOFF			3000		//  12db
int AudioStream::ServiceBuffer (void)
{
	long	vol;
	int	fRtn = TRUE;

	if ( status != ASF_USED )
		return FALSE;

	SDL_LockMutex(write_lock);

	// status may have changed, so lets check once again
	if ( status != ASF_USED ){
		SDL_UnlockMutex(write_lock);
		return FALSE;
	}

	if ( m_bFade == TRUE ) {
		if ( m_lCutoffVolume == -10000 ) {
			vol = Get_Volume();
//			nprintf(("Alan","Volume is: %d\n",vol));
			m_lCutoffVolume = vol - VOLUME_ATTENUATION_BEFORE_CUTOFF;
			if ( m_lCutoffVolume < -10000 )
				m_lCutoffVolume = -10000;
		}

		vol = Get_Volume();
		vol = vol - FADE_VOLUME_INTERVAL;	// decrease by 1db
//		nprintf(("Alan","Volume is now: %d\n",vol));
		Set_Volume(vol);

//		nprintf(("Sound","SOUND => Volume for stream sound is %d\n",vol));
//		nprintf(("Alan","Cuttoff Volume is: %d\n",m_lCutoffVolume));
		if ( vol < m_lCutoffVolume ) {
			m_bFade = 0;
			m_lCutoffVolume = -10000;
			if ( m_bDestroy_when_faded == TRUE ) {
				SDL_UnlockMutex(write_lock);
				Destroy();
				return FALSE;
			}
			else {
				Stop_and_Rewind();
				SDL_UnlockMutex(write_lock);
				return TRUE;
			}
		}
	}

	// All of sound not played yet, send more data to buffer
	uint dwFreeSpace = GetMaxWriteSize ();

	// Determine free space in sound buffer
	if (dwFreeSpace) {

		// Some wave data remains, but not enough to fill free space
		// Send wave data to buffer, fill remainder of free space with silence
		uint num_bytes_written;

		if (WriteWaveData (dwFreeSpace, &num_bytes_written) == SUCCESS) {
//			nprintf(("Alan","Num bytes written: %d\n", num_bytes_written));

			if ( m_pwavefile->m_total_uncompressed_bytes_read >= m_pwavefile->m_max_uncompressed_bytes_to_read ) {
				m_fade_timer_id = timer_get_milliseconds() + 1700;		// start fading 1.7 seconds from now
				m_finished_id = timer_get_milliseconds() + 2000;		// 2 seconds left to play out buffer
				m_pwavefile->m_max_uncompressed_bytes_to_read = AS_HIGHEST_MAX;
			}

			if ( (m_fade_timer_id>0) && ((uint)timer_get_milliseconds() > m_fade_timer_id) ) {
				m_fade_timer_id = 0;
				Fade_and_Stop();
			}

			if ( (m_finished_id>0) && ((uint)timer_get_milliseconds() > m_finished_id) ) {
				m_finished_id = 0;
				m_bPastLimit = TRUE;
			}

			// see if we're done: nothing left to read, and every queued buffer
			// has been played through
			ALint n = 0, q = 0;

			alGetError();
			alGetSourcei(m_source_id, AL_BUFFERS_PROCESSED, &n);
			if ( openal_error("AL_BUFFERS_PROCESSED") )
				m_bReadingDone = TRUE;

			alGetSourcei(m_source_id, AL_BUFFERS_QUEUED, &q);

			if ( m_bReadingDone && (n == q) ) {
				if ( m_bDestroy_when_faded == TRUE ) {
					SDL_UnlockMutex(write_lock);
					Destroy();
					return FALSE;
				}

				// All of sound has played, stop playback or loop again
				if ( m_bLooping && !m_bFade) {
					Play(m_lVolume, m_bLooping);
				}
				else {
					Stop_and_Rewind();
				}
			}
		}
		else {
			// Error writing wave data
			fRtn = FALSE;
			Int3();
		}
	}

	SDL_UnlockMutex(write_lock);
	return (fRtn);
}

// Cue
void AudioStream::Cue (void)
{
	uint num_bytes_written;

	if (!m_fCued) {
		m_bFade = FALSE;
		m_fade_timer_id = 0;
		m_finished_id = 0;
		m_bPastLimit = FALSE;
		m_lVolume = 0;
		m_lCutoffVolume = -10000;

		m_bDestroy_when_faded = FALSE;

		// Reset buffer ptr
		m_cbBufOffset = 0;

		// Reset file ptr, etc
		m_pwavefile->Cue ();

		// Unqueue any left-over processed buffers
		ALint p = 0;
		alGetSourcei(m_source_id, AL_BUFFERS_PROCESSED, &p);

		if (p > 0)
			alSourceUnqueueBuffers(m_source_id, p, m_buffer_ids);

		// Fill first buffer with wave data
		WriteWaveData (m_cbBufSize, &num_bytes_written,0);

		m_fCued = TRUE;
	}
}


// Play
void AudioStream::Play (long volume, int looping)
{
	if (m_buffer_ids[0] != 0) {
		// If playing, stop
		if (m_fPlaying) {
			if ( m_bIsPaused == FALSE)
			Stop_and_Rewind();
		}

		// Cue for playback if necessary
		if (!m_fCued) {
			Cue ();
		}

		if ( looping )
			m_bLooping = 1;
		else
			m_bLooping = 0;

		// Begin OpenAL playback
		alSourcePlay(m_source_id);
		openal_error("alSourcePlay");

		m_nTimeStarted = timer_get_milliseconds();
		Set_Volume(volume);

		// Kick off timer to service buffer
		m_timer.constructor();

		m_timer.Create (m_nBufService, m_nBufService, (void *)this, TimerCallback);

		// Playback begun, no longer cued
		m_fPlaying = TRUE;
		m_bIsPaused = FALSE;
	}
}

// Timer callback for Timer object created by ::Play method.
int AudioStream::TimerCallback (void *dwUser)
{
    // dwUser contains ptr to AudioStream object
    AudioStream * pas = (AudioStream *) dwUser;

    return (pas->ServiceBuffer ());
}

void AudioStream::Set_Byte_Cutoff(unsigned int byte_cutoff)
{
	if ( m_pwavefile == NULL )
		return;

	m_pwavefile->m_max_uncompressed_bytes_to_read = byte_cutoff;
}

unsigned int AudioStream::Get_Bytes_Committed(void)
{
	if ( m_pwavefile == NULL )
		return 0;

	return m_pwavefile->m_total_uncompressed_bytes_read;
}


// Fade_and_Destroy
void AudioStream::Fade_and_Destroy (void)
{
	m_bFade = TRUE;
	m_bDestroy_when_faded = TRUE;
}

// Fade_and_Destroy
void AudioStream::Fade_and_Stop (void)
{
	m_bFade = TRUE;
	m_bDestroy_when_faded = FALSE;
}


// Stop
void AudioStream::Stop(int paused)
{
	if (m_fPlaying) {
		if (paused) {
			alSourcePause(m_source_id);
		} else {
			alSourceStop(m_source_id);
		}
		openal_error("Stop");

		m_fPlaying = FALSE;
		m_bIsPaused = paused;

		// Delete Timer object
		m_timer.destructor();
	}
}

// Stop_and_Rewind
void AudioStream::Stop_and_Rewind (void)
{
	if (m_fPlaying) {
		// Stop playback
		alSourceStop(m_source_id);
		openal_error("Stop_and_Rewind");

		// Delete Timer object
		m_timer.destructor();

		m_fPlaying = FALSE;
	}

	// Unqueue all processed buffers
	ALint p = 0;
	alGetSourcei(m_source_id, AL_BUFFERS_PROCESSED, &p);

	if (p > 0)
		alSourceUnqueueBuffers(m_source_id, p, m_buffer_ids);

	m_fCued = FALSE;	// this will cause wave file to start from beginning
	m_bReadingDone = FALSE;
}

// Set_Volume
void AudioStream::Set_Volume(long vol)
{
	if ( vol < -10000 )
		vol = -10000;

	if ( vol > 0 )
		vol = 0;

	Assert( vol >= -10000 && vol <= 0 );

	// convert the DirectSound-style hundredths-of-decibels volume to a linear
	// OpenAL gain
	ALfloat alvol = (vol != -10000) ? powf(10.0f, (float)vol / (-600.0f / log10f(.5f))) : 0.0f;

	alSourcef(m_source_id, AL_GAIN, alvol);
	openal_error("Set_Volume");

	m_lVolume = vol;
}


// Set_Volume
long AudioStream::Get_Volume()
{
	return m_lVolume;
}

// constructor
void Timer::constructor(void)
{
	m_nIDTimer = 0;
}


// Destructor
void Timer::destructor(void)
{
	if (m_nIDTimer) {
		SDL_RemoveTimer(m_nIDTimer);
		m_nIDTimer = 0;
	}
}


// Create
int Timer::Create (uint nPeriod, uint nRes, void *dwUser, TIMERCALLBACK pfnCallback)
{
	int bRtn = SUCCESS;    // assume success

	Assert(pfnCallback);
	Assert(nPeriod > 10);
	Assert(nPeriod >= nRes);

	m_nPeriod = nPeriod;
	m_nRes = nRes;
	m_dwUser = dwUser;
	m_pfnCallback = pfnCallback;

	if ((m_nIDTimer = SDL_AddTimer (m_nPeriod, TimeProc, (void *)this)) == 0) {
	  bRtn = FAILURE;
	}

	return (bRtn);
}


// Timer proc for the periodic timer callback set with SDL_AddTimer().
// Runs on the SDL timer thread.
//
// Calls procedure specified when Timer object was created. The
// param parameter contains "this" pointer for associated Timer object.
//
Uint32 Timer::TimeProc(Uint32 interval, void *param)
{
    // param contains ptr to Timer object
    Timer * ptimer = (Timer *) param;

    // Call user-specified callback and pass back user specified data
    (ptimer->m_pfnCallback) (ptimer->m_dwUser);

    // rearm for the next interval
    return interval;
}


// WaveFile class implementation
//
////////////////////////////////////////////////////////////

// Constructor
void WaveFile::Init(void)
{
	// Init data members
	m_data_offset = 0;
	cfp = NULL;
	m_pwfmt_original = NULL;
	m_nBlockAlign= 0;
	m_nUncompressedAvgDataRate = 0;
	m_nDataSize = 0;
	m_nBytesPlayed = 0;
	m_total_uncompressed_bytes_read = 0;
	m_max_uncompressed_bytes_to_read = AS_HIGHEST_MAX;
	m_ALformat = AL_FORMAT_MONO16;

	memset(m_wFilename, 0, MAX_FILENAME_LEN);

	m_hStream_open = 0;
	m_abort_next_read = FALSE;
}

// Destructor
void WaveFile::Close(void)
{
	// Free memory
	if (m_pwfmt_original) {
		free(m_pwfmt_original);
		m_pwfmt_original = NULL;
	}

	if ( m_hStream_open ) {
		ACM_stream_close((void*)m_hStream);
		m_hStream_open = 0;
	}

	// Close file
	if (cfp) {
		fclose( cfp );
		cfp = NULL;
	}
}


// Open
int WaveFile::Open (char *pszFilename)
{
	int done = FALSE;
	WORD cbExtra = 0;
	int fRtn = SUCCESS;    // assume success
	WAVEFORMATEX pcmwf;
	char fullpath[MAX_PATH_LEN];

	m_total_uncompressed_bytes_read = 0;
	m_max_uncompressed_bytes_to_read = AS_HIGHEST_MAX;

	int FileSize, FileOffset;

	// audio streams are read straight out of the VP archive: find the real
	// file on disk and the offset of this file within it
	if ( !cf_find_file_location(pszFilename, CF_TYPE_ANY, fullpath, &FileSize, &FileOffset ))	{
		goto OPEN_ERROR;
	}

	cfp = fopen(fullpath, "rb");
	if ( cfp == NULL ) {
		goto OPEN_ERROR;
	}

	// Skip the "RIFF" tag and file size (8 bytes)
	// Skip the "WAVE" tag (4 bytes)
	fseek( cfp, 12+FileOffset, SEEK_SET );

	// Now read RIFF tags until the end of file
	uint tag, size, next_chunk;

	while(done == FALSE)	{
		if ( !audiostr_read_uint(cfp, &tag) )
			break;

		if ( !audiostr_read_uint(cfp, &size) )
			break;

		next_chunk = ftell( cfp );
		next_chunk += size;

		switch( tag )	{
		case 0x20746d66:		// The 'fmt ' tag
			audiostr_read_word( cfp, &pcmwf.wFormatTag );
			audiostr_read_word( cfp, &pcmwf.nChannels );
			audiostr_read_dword( cfp, &pcmwf.nSamplesPerSec );
			audiostr_read_dword( cfp, &pcmwf.nAvgBytesPerSec );
			audiostr_read_word( cfp, &pcmwf.nBlockAlign );
			audiostr_read_word( cfp, &pcmwf.wBitsPerSample );

			if ( pcmwf.wFormatTag != WAVE_FORMAT_PCM ) {
				audiostr_read_word( cfp, &cbExtra );
			}

			// Allocate memory for WAVEFORMATEX structure + extra bytes
			if ( (m_pwfmt_original = (WAVEFORMATEX *) malloc ( sizeof(WAVEFORMATEX)+cbExtra )) != NULL ){
				// Copy fields from temporary format structure
				*m_pwfmt_original = pcmwf;
				m_pwfmt_original->cbSize = cbExtra;

				// Read those extra bytes, append to WAVEFORMATEX structure.
				// They go at the on-disk offset, right behind the 18 header
				// bytes, where the ACM decoder expects them
				if (cbExtra != 0) {
					fread( (ubyte *)(m_pwfmt_original) + WAVEFORMATEX_DISK_SIZE, 1, cbExtra, cfp );
				}
			}
			else {
				Int3();		// malloc failed
				goto OPEN_ERROR;
			}
			break;

		case 0x61746164:		// the 'data' tag
			m_nDataSize = size;	// This is size of data chunk.  Compressed if ADPCM.
			m_data_bytes_left = size;
			m_data_offset = ftell( cfp );
			done = TRUE;
			break;

		default:	// unknown, skip it
			break;
		}	// end switch

		fseek( cfp, next_chunk, SEEK_SET );
	}

	// make sure we found both the 'fmt ' and 'data' chunks
	if ( !done || (m_pwfmt_original == NULL) ) {
		goto OPEN_ERROR;
	}

  	// At this stage, examine source format, and set up the WAVEFORMATEX
	// structure for the sound system.  OpenAL only takes PCM, so force this
	// structure to be PCM compliant.  We will need to convert data on the fly
	// later if our source is not PCM
	switch ( m_pwfmt_original->wFormatTag ) {
		case WAVE_FORMAT_PCM:
			m_wave_format = WAVE_FORMAT_PCM;
			m_wfmt.wBitsPerSample = m_pwfmt_original->wBitsPerSample;
			break;

		case WAVE_FORMAT_ADPCM:
			m_wave_format = WAVE_FORMAT_ADPCM;
			m_wfmt.wBitsPerSample = 16;
			m_bits_per_sample_uncompressed = 16;
			break;

		default:
			nprintf(("SOUND", "SOUND => Not supporting %d format for playing wave files\n", m_pwfmt_original->wFormatTag));
			//Int3();
			goto OPEN_ERROR;
			break;

	} // end switch

	// Set up the WAVEFORMATEX structure to have the right PCM characteristics
	m_wfmt.wFormatTag = WAVE_FORMAT_PCM;
	m_wfmt.nChannels = m_pwfmt_original->nChannels;
	m_wfmt.nSamplesPerSec = m_pwfmt_original->nSamplesPerSec;
	m_wfmt.cbSize = 0;
	m_wfmt.nBlockAlign = (unsigned short)(( m_wfmt.nChannels * m_wfmt.wBitsPerSample ) / 8);
	m_wfmt.nAvgBytesPerSec = m_wfmt.nBlockAlign * m_wfmt.nSamplesPerSec;

	// Init some member data from format chunk
	m_nBlockAlign = m_pwfmt_original->nBlockAlign;
	m_nUncompressedAvgDataRate = m_wfmt.nAvgBytesPerSec;

	Assert( (m_wfmt.nChannels == 1) || (m_wfmt.nChannels == 2) );

	// pick the matching OpenAL buffer format
	switch ( m_wfmt.wBitsPerSample ) {
		case 8:
			m_ALformat = (m_wfmt.nChannels == 2) ? AL_FORMAT_STEREO8 : AL_FORMAT_MONO8;
			break;

		case 16:
			m_ALformat = (m_wfmt.nChannels == 2) ? AL_FORMAT_STEREO16 : AL_FORMAT_MONO16;
			break;

		default:
			Int3();
			goto OPEN_ERROR;
	}

	// Cue for streaming
	Cue ();

	// Successful open
	goto OPEN_DONE;

OPEN_ERROR:
	// Handle all errors here
	nprintf(("SOUND","SOUND ==> Could not open wave file %s for streaming\n",pszFilename));

	fRtn = FAILURE;
	if (cfp != NULL) {
		// Close file
		fclose( cfp );
		cfp = NULL;
	}
	if (m_pwfmt_original)
	{
		free(m_pwfmt_original);
		m_pwfmt_original = NULL;
	}

OPEN_DONE:
	strncpy(m_wFilename, pszFilename, MAX_FILENAME_LEN-1);
	return (fRtn);
}


// Cue
//
// Set the file pointer to the start of wave data
//
int WaveFile::Cue (void)
{
	int fRtn = SUCCESS;    // assume success
	int rval;

	m_total_uncompressed_bytes_read = 0;
	m_max_uncompressed_bytes_to_read = AS_HIGHEST_MAX;

	rval = fseek( cfp, m_data_offset, SEEK_SET );
	if ( rval != 0 ) {
		fRtn = FAILURE;
	}

	m_data_bytes_left = m_nDataSize;
	m_abort_next_read = FALSE;

	return fRtn;
}


// Read
//
// Returns number of bytes actually read.
//
//	Returns -1 if there is nothing more to be read.  This function can return 0, since
// sometimes the amount of bytes requested is too small for the ACM decompression to
// locate a suitable block
int WaveFile::Read(ubyte *pbDest, uint cbSize, int service)
{
	unsigned char	*dest_buf=NULL, *uncompressed_wave_data;
	int				rc, uncompressed_bytes_written;
	unsigned int	src_bytes_used, convert_len, num_bytes_desired=0, num_bytes_read;

//	nprintf(("Alan","Reqeusted: %d\n", cbSize));


	if ( service ) {
		uncompressed_wave_data = Wavedata_service_buffer;
	} else {
		uncompressed_wave_data = Wavedata_load_buffer;
	}

	switch ( m_wave_format ) {
		case WAVE_FORMAT_PCM:
			num_bytes_desired = cbSize;
			dest_buf = pbDest;
			break;

		case WAVE_FORMAT_ADPCM:
			if ( !m_hStream_open ) {
				if ( !ACM_stream_open(m_pwfmt_original, &m_wfxDest, (void**)&m_hStream, m_bits_per_sample_uncompressed)  ) {
					m_hStream_open = 1;
				} else {
					Int3();
				}
			}

			num_bytes_desired = cbSize;

			if ( service ) {
				dest_buf = Compressed_service_buffer;
			} else {
				dest_buf = Compressed_buffer;
			}

			if ( num_bytes_desired <= 0 ) {
				num_bytes_desired = 0;
//				nprintf(("Alan","No bytes required for ADPCM time interval\n"));
			} else {
				num_bytes_desired = ACM_query_source_size((void*)m_hStream, cbSize);
//				nprintf(("Alan","Num bytes desired: %d\n", num_bytes_desired));
			}
			break;

		default:
			nprintf(("SOUND", "SOUND => Not supporting %d format for playing wave files\n", m_wave_format));
			Int3();
			break;

	} // end switch

	num_bytes_read = 0;
	convert_len = 0;
	src_bytes_used = 0;

	// read data from disk
	if ( m_data_bytes_left <= 0 ) {
		num_bytes_read = 0;
		uncompressed_bytes_written = 0;
		return -1;
	}

	if ( m_data_bytes_left > 0 && num_bytes_desired > 0 ) {
		int actual_read;

		if ( num_bytes_desired <= (unsigned int)m_data_bytes_left ) {
			num_bytes_read = num_bytes_desired;
		}
		else {
			num_bytes_read = m_data_bytes_left;
		}

		actual_read = (int)fread( dest_buf, 1, num_bytes_read, cfp );
		if ( (actual_read <= 0) || (m_abort_next_read) ) {
			num_bytes_read = 0;
			uncompressed_bytes_written = 0;
			return -1;
		}

		if ( num_bytes_desired >= (unsigned int)m_data_bytes_left ) {
			m_abort_next_read = 1;
		}

		num_bytes_read = actual_read;
	}

	// convert data if necessary, to PCM
	if ( m_wave_format == WAVE_FORMAT_ADPCM ) {
		if ( num_bytes_read > 0 ) {
				rc = ACM_convert((void*)m_hStream, dest_buf, num_bytes_read, uncompressed_wave_data, BIGBUF_SIZE, &convert_len, &src_bytes_used);
				if ( rc == -1 ) {
					goto READ_ERROR;
				}
				if ( convert_len == 0 ) {
					if ( num_bytes_read < m_nBlockAlign ) {
						nprintf(("SOUND", "SOUND => Warning: short read detected in ACM decode of '%s'\n", m_wFilename));
					} else {
						Int3();
					}
				}
		}

		Assert(src_bytes_used <= num_bytes_read);
		if ( src_bytes_used < num_bytes_read ) {
			// seek back file pointer to reposition before unused source data
			fseek(cfp, (int)src_bytes_used - (int)num_bytes_read, SEEK_CUR);
		}

		// Adjust number of bytes left
		m_data_bytes_left -= src_bytes_used;
		m_nBytesPlayed += src_bytes_used;
		uncompressed_bytes_written = convert_len;

		// Successful read, keep running total of number of data bytes read
		goto READ_DONE;
	}
	else {
		// Successful read, keep running total of number of data bytes read
		// Adjust number of bytes left
		m_data_bytes_left -= num_bytes_read;
		m_nBytesPlayed += num_bytes_read;
		uncompressed_bytes_written = num_bytes_read;
		goto READ_DONE;
	}

READ_ERROR:
	num_bytes_read = 0;
	uncompressed_bytes_written = 0;

READ_DONE:
	m_total_uncompressed_bytes_read += uncompressed_bytes_written;
//	nprintf(("Alan","Read: %d\n", uncompressed_bytes_written));
	return (uncompressed_bytes_written);
}


// GetSilenceData
//
// Returns 8 bits of data representing silence for the Wave file format.
//
// Since we are dealing only with PCM format, we can fudge a bit and take
// advantage of the fact that for all PCM formats, silence can be represented
// by a single byte, repeated to make up the proper word size. The actual size
// of a word of wave data depends on the format:
//
// PCM Format       Word Size       Silence Data
// 8-bit mono       1 byte          0x80
// 8-bit stereo     2 bytes         0x8080
// 16-bit mono      2 bytes         0x0000
// 16-bit stereo    4 bytes         0x00000000
//
ubyte WaveFile::GetSilenceData (void)
{
	ubyte bSilenceData = 0;

	// Silence data depends on format of Wave file
	if (m_pwfmt_original) {
		if (m_wfmt.wBitsPerSample == 8) {
			// For 8-bit formats (unsigned, 0 to 255)
			// Packed DWORD = 0x80808080;
			bSilenceData = 0x80;
		}
		else if (m_wfmt.wBitsPerSample == 16) {
			// For 16-bit formats (signed, -32768 to 32767)
			// Packed DWORD = 0x00000000;
			bSilenceData = 0x00;
		}
		else {
			Int3();
		}
	}
	else {
		Int3();
	}

	return (bSilenceData);
}

int Audiostream_inited = 0;

#define MAX_AUDIO_STREAMS	30
AudioStream Audio_streams[MAX_AUDIO_STREAMS];

void audiostream_init()
{
	int i;

	if ( Audiostream_inited == 1 )
		return;

	if ( !ACM_is_inited() ) {
		return;
	}

	// Allocate memory for the buffer which holds the uncompressed wave data that is streamed from the
	// disk during a load/cue
	if ( Wavedata_load_buffer == NULL ) {
		Wavedata_load_buffer = (unsigned char*)malloc(BIGBUF_SIZE);
		Assert(Wavedata_load_buffer != NULL);
	}

	// Allocate memory for the buffer which holds the uncompressed wave data that is streamed from the
	// disk during a service interval
	if ( Wavedata_service_buffer == NULL ) {
		Wavedata_service_buffer = (unsigned char*)malloc(BIGBUF_SIZE);
		Assert(Wavedata_service_buffer != NULL);
	}

	// Allocate memory for the buffer which holds the compressed wave data that is read from the hard disk
	if ( Compressed_buffer == NULL ) {
		Compressed_buffer = (unsigned char*)malloc(COMPRESSED_BUFFER_SIZE);
		Assert(Compressed_buffer != NULL);
	}

	if ( Compressed_service_buffer == NULL ) {
		Compressed_service_buffer = (unsigned char*)malloc(COMPRESSED_BUFFER_SIZE);
		Assert(Compressed_service_buffer != NULL);
	}

	for ( i = 0; i < MAX_AUDIO_STREAMS; i++ ) {
		Audio_streams[i].Init_Data();
		Audio_streams[i].status = ASF_FREE;
		Audio_streams[i].type = ASF_NONE;
	}

	// the service timer runs on the SDL timer thread
	SDL_InitSubSystem(SDL_INIT_TIMER);

	Global_service_lock = SDL_CreateMutex();

	Audiostream_inited = 1;
}

// Close down the audiostream system.  Must call audiostream_init() before any audiostream functions can
// be used.
void audiostream_close()
{
	int i;
	if ( Audiostream_inited == 0 )
		return;

	for ( i = 0; i < MAX_AUDIO_STREAMS; i++ ) {
		if ( Audio_streams[i].status == ASF_USED ) {
			Audio_streams[i].status = ASF_FREE;
			Audio_streams[i].Destroy();
		}
	}

	// free global buffers
	if ( Wavedata_load_buffer ) {
		free(Wavedata_load_buffer);
		Wavedata_load_buffer = NULL;
	}

	if ( Wavedata_service_buffer ) {
		free(Wavedata_service_buffer);
		Wavedata_service_buffer = NULL;
	}

	if ( Compressed_buffer ) {
		free(Compressed_buffer);
		Compressed_buffer = NULL;
	}

	if ( Compressed_service_buffer ) {
		free(Compressed_service_buffer);
		Compressed_service_buffer = NULL;
	}

	SDL_DestroyMutex( Global_service_lock );
	Global_service_lock = NULL;

	Audiostream_inited = 0;
}

// Open a digital sound file for streaming
//
// input:	filename	=>	disk filename of sound file
//				type		=> what type of audio stream do we want to open:
//									ASF_SOUNDFX
//									ASF_EVENTMUSIC
//									ASF_VOICE
//
// returns:	success => handle to identify streaming sound
//				failure => -1
int audiostream_open( char * filename, int type )
{
	int i, rc;
	if (!Audiostream_inited || !snd_is_inited())
		return -1;

	for ( i = 0; i < MAX_AUDIO_STREAMS; i++ ) {
		if ( Audio_streams[i].status == ASF_FREE ) {
			Audio_streams[i].status = ASF_USED;
			Audio_streams[i].type = type;
			break;
		}
	}

	if ( i == MAX_AUDIO_STREAMS ) {
		nprintf(("Sound", "SOUND => No more audio streams available!\n"));
		return -1;
	}

	switch(type) {
	case ASF_VOICE:
	case ASF_SOUNDFX:
	case ASF_EVENTMUSIC:
		// we always uncompress to 16 bits
		Audio_streams[i].m_bits_per_sample_uncompressed = 16;
		break;
	default:
		Int3();
		Audio_streams[i].status = ASF_FREE;
		return -1;
	}

	rc = Audio_streams[i].Create(filename);
	if ( rc == 0 ) {
		Audio_streams[i].status = ASF_FREE;
		return -1;
	}
	else
		return i;
}


void audiostream_close_file(int i, int fade)
{
	if (!Audiostream_inited)
		return;

	if ( i == -1 )
		return;

	Assert( i >= 0 && i < MAX_AUDIO_STREAMS );

	if ( Audio_streams[i].status == ASF_USED ) {
		if ( fade == TRUE ) {
			Audio_streams[i].Fade_and_Destroy();
		}
		else {
			Audio_streams[i].Destroy();
		}
	}
}

void audiostream_close_all(int fade)
{
	int i;

	for ( i = 0; i < MAX_AUDIO_STREAMS; i++ ) {
		if ( Audio_streams[i].status == ASF_FREE )
			continue;

		audiostream_close_file(i, fade);
	}
}

void audiostream_play(int i, float volume, int looping)
{
	if (!Audiostream_inited)
		return;

	if ( i == -1 )
		return;

	Assert(looping >= 0);
	Assert( i >= 0 && i < MAX_AUDIO_STREAMS );

	// convert from 0->1 to -10000->0 for volume
	int converted_volume;
	if ( volume == -1 ) {
		converted_volume = Audio_streams[i].Get_Default_Volume();
	}
	else {
		Assert(volume >= 0.0f && volume <= 1.0f );
		converted_volume = ds_convert_volume(volume);
	}

	Assert( Audio_streams[i].status == ASF_USED );
	Audio_streams[i].Set_Default_Volume(converted_volume);
	Audio_streams[i].Play(converted_volume, looping);
}

void audiostream_stop(int i, int rewind, int paused)
{
	if (!Audiostream_inited) return;

	if ( i == -1 )
		return;

	Assert( i >= 0 && i < MAX_AUDIO_STREAMS );
	Assert( Audio_streams[i].status == ASF_USED );

	if ( rewind )
		Audio_streams[i].Stop_and_Rewind();
	else
		Audio_streams[i].Stop(paused);
}

int audiostream_is_playing(int i)
{
	if ( i == -1 )
		return 0;

	Assert( i >= 0 && i < MAX_AUDIO_STREAMS );
	if ( Audio_streams[i].status != ASF_USED )
		return 0;

	return Audio_streams[i].Is_Playing();
}


void audiostream_set_volume_all(float volume, int type)
{
	int i;

	for ( i = 0; i < MAX_AUDIO_STREAMS; i++ ) {
		if ( Audio_streams[i].status == ASF_FREE )
			continue;

		if ( Audio_streams[i].type == type ) {
			int converted_volume;
			converted_volume = ds_convert_volume(volume);
			Audio_streams[i].Set_Volume(converted_volume);
		}
	}
}


void audiostream_set_volume(int i, float volume)
{
	if ( i == -1 )
		return;

	Assert( i >= 0 && i < MAX_AUDIO_STREAMS );
	Assert( volume >= 0 && volume <= 1);

	if ( Audio_streams[i].status == ASF_FREE )
		return;

	int converted_volume;
	converted_volume = ds_convert_volume(volume);
	Audio_streams[i].Set_Volume(converted_volume);
}


int audiostream_is_paused(int i)
{
	if ( i == -1 )
		return 0;

	Assert( i >= 0 && i < MAX_AUDIO_STREAMS );
	if ( Audio_streams[i].status == ASF_FREE )
		return -1;

	int is_paused;
	is_paused = Audio_streams[i].Is_Paused();
	return is_paused;
}


void audiostream_set_byte_cutoff(int i, unsigned int cutoff)
{
	if ( i == -1 )
		return;

	Assert( i >= 0 && i < MAX_AUDIO_STREAMS );
	Assert( cutoff > 0 );

	if ( Audio_streams[i].status == ASF_FREE )
		return;

	Audio_streams[i].Set_Byte_Cutoff(cutoff);
}


unsigned int audiostream_get_bytes_committed(int i)
{
	if ( i == -1 )
		return 0;

	Assert( i >= 0 && i < MAX_AUDIO_STREAMS );

	if ( Audio_streams[i].status == ASF_FREE )
		return 0;

	unsigned int num_bytes_committed;
	num_bytes_committed = Audio_streams[i].Get_Bytes_Committed();
	return num_bytes_committed;
}

int audiostream_done_reading(int i)
{
	if ( i == -1 )
		return 0;

	Assert( i >= 0 && i < MAX_AUDIO_STREAMS );

	if ( Audio_streams[i].status == ASF_FREE )
		return 0;

	int done_reading;
	done_reading = Audio_streams[i].Is_Past_Limit();
	return done_reading;
}


int audiostream_is_inited()
{
	return Audiostream_inited;
}

// pause a single audio stream, indentified by handle i.
void audiostream_pause(int i)
{
	if ( i == -1 )
		return;

	Assert( i >= 0 && i < MAX_AUDIO_STREAMS );
	if ( Audio_streams[i].status == ASF_FREE )
		return;

	if ( audiostream_is_playing(i) == TRUE ) {
		audiostream_stop(i, 0, 1);
	}
}

// pause all audio streams that are currently playing.
void audiostream_pause_all()
{
	int i;

	for ( i = 0; i < MAX_AUDIO_STREAMS; i++ ) {
		if ( Audio_streams[i].status == ASF_FREE )
			continue;

		audiostream_pause(i);
	}
}

// unpause the audio stream identified by handle i.
void audiostream_unpause(int i)
{
	int is_looping;

	if ( i == -1 )
		return;

	Assert( i >= 0 && i < MAX_AUDIO_STREAMS );
	if ( Audio_streams[i].status == ASF_FREE )
		return;

	if ( audiostream_is_paused(i) == TRUE ) {
		is_looping = Audio_streams[i].Is_looping();
		audiostream_play(i, -1.0f, is_looping);
	}
}

// unpause all audio streams that are currently paused
void audiostream_unpause_all()
{
	int i;

	for ( i = 0; i < MAX_AUDIO_STREAMS; i++ ) {
		if ( Audio_streams[i].status == ASF_FREE )
			continue;

		audiostream_unpause(i);
	}
}
