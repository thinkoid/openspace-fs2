// -*- mode: c++; -*-

#ifndef FREESPACE2_SOUND_OPENAL_HH
#define FREESPACE2_SOUND_OPENAL_HH

#include <defs.hh>

#include <AL/al.h>
#include <AL/alc.h>

#include <string>

const char* openal_error_string (int get_alc = 0);
bool openal_init_device (std::string* playback, std::string* capture);

ALenum openal_get_format (ALint bits, ALint n_channels);

struct OpenALInformation {
    uint32_t version_major = 0;
    uint32_t version_minor = 0;

    std::string default_playback_device{};
    std::string default_capture_device{};

    std::vector< std::string > playback_devices;
    std::vector< std::string > capture_devices;

    std::vector< std::pair< std::string, bool > > efx_support;
};

OpenALInformation openal_get_platform_information ();

#define OpenAL_ErrorCheck( x, y )   do {                        \
    x;                                                          \
    const char* err = openal_error_string(0);                   \
    if (err && err [0]) {                                       \
        ERRORF (LOCATION, "SOUND: OpenAL error: '%s'", err);    \
        y;                                                      \
    }                                                           \
} while (0)

#define OpenAL_ErrorPrint( x )  do {                    \
    x;                                                  \
    const char *err = openal_error_string(0);           \
    if (err != NULL ) {                                 \
        ERRORF (LOCATION, "OpenAL error: \"%s\"", err); \
    }                                                   \
} while (0)

#define OpenAL_C_ErrorCheck( x, y ) do {                        \
    x;                                                          \
    const char *err = openal_error_string(1);                   \
    if ( err != NULL ) {                                        \
        ERRORF (LOCATION, "SOUND: OpenAL error: '%s'\n", err);  \
        y;                                                      \
    }                                                           \
} while (0)

#define OpenAL_C_ErrorPrint( x )    do {                    \
    x;                                                      \
    const char *err = openal_error_string(1);               \
    if ( err != NULL ) {                                    \
        WARNINGF (LOCATION, "OpenAL ERROR: \"%s\"", err);   \
    }                                                       \
} while (0)

#ifndef AL_BYTE_LOKI

#define AL_BYTE_LOKI    0x100C
#endif


#ifndef AL_BYTE_OFFSET
#define AL_BYTE_OFFSET  0x1026
#endif

#ifndef ALC_EXT_EFX
#define ALC_EXT_EFX  1
#define AL_FILTER_TYPE                  0x8001
#define AL_EFFECT_TYPE                  0x8001
#define AL_FILTER_NULL                  0x0000
#define AL_FILTER_LOWPASS               0x0001
#define AL_EFFECT_NULL                  0x0000
#define AL_EFFECT_EAXREVERB             0x8000
#define AL_EFFECT_REVERB                0x0001
#define AL_EFFECT_ECHO                  0x0004
#define ALC_EFX_MAJOR_VERSION           0x20001
#define ALC_EFX_MINOR_VERSION           0x20002
#define ALC_MAX_AUXILIARY_SENDS         0x20003

#define AL_AUXILIARY_SEND_FILTER        0x20006

#define AL_EAXREVERB_DENSITY                    0x0001
#define AL_EAXREVERB_DIFFUSION                  0x0002
#define AL_EAXREVERB_GAIN                       0x0003
#define AL_EAXREVERB_GAINHF                     0x0004
#define AL_EAXREVERB_GAINLF                     0x0005
#define AL_EAXREVERB_DECAY_TIME                 0x0006
#define AL_EAXREVERB_DECAY_HFRATIO              0x0007
#define AL_EAXREVERB_DECAY_LFRATIO              0x0008
#define AL_EAXREVERB_REFLECTIONS_GAIN           0x0009
#define AL_EAXREVERB_REFLECTIONS_DELAY          0x000A
#define AL_EAXREVERB_REFLECTIONS_PAN            0x000B
#define AL_EAXREVERB_LATE_REVERB_GAIN           0x000C
#define AL_EAXREVERB_LATE_REVERB_DELAY          0x000D
#define AL_EAXREVERB_LATE_REVERB_PAN            0x000E
#define AL_EAXREVERB_ECHO_TIME                  0x000F
#define AL_EAXREVERB_ECHO_DEPTH                 0x0010
#define AL_EAXREVERB_MODULATION_TIME            0x0011
#define AL_EAXREVERB_MODULATION_DEPTH           0x0012
#define AL_EAXREVERB_AIR_ABSORPTION_GAINHF      0x0013
#define AL_EAXREVERB_HFREFERENCE                0x0014
#define AL_EAXREVERB_LFREFERENCE                0x0015
#define AL_EAXREVERB_ROOM_ROLLOFF_FACTOR        0x0016
#define AL_EAXREVERB_DECAY_HFLIMIT              0x0017

#define AL_EFFECTSLOT_NULL                      0x0000
#define AL_EFFECTSLOT_EFFECT                    0x0001
#endif

#ifndef AL_EXT_float32
#define AL_EXT_float32 1
#define AL_FORMAT_MONO_FLOAT32          0x10010
#define AL_FORMAT_STEREO_FLOAT32        0x10011
#endif

#endif 
