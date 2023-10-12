// -*- mode: c++; -*-

#include <string>
#include <algorithm>

#include <defs.hh>
#include <shared/types.hh>
#include <sound/openal.hh>
#include <osapi/osregistry.hh>
#include <log/log.hh>

static std::string Playback_device;
static std::string Capture_device;

enum { OAL_DEVICE_GENERIC, OAL_DEVICE_DEFAULT, OAL_DEVICE_USER };

typedef struct OALdevice {
    std::string device_name;
    int type;
    bool usable;

    OALdevice () : type (OAL_DEVICE_GENERIC), usable (false) {}

    OALdevice (const char* name)
        : device_name (name), type (OAL_DEVICE_GENERIC), usable (false) {}
} OALdevice;

static std::vector< OALdevice > PlaybackDevices;
static std::vector< OALdevice > CaptureDevices;

// enumeration extension
#ifndef ALC_ALL_DEVICES_SPECIFIER
#define ALC_ALL_DEVICES_SPECIFIER 0x1013
#endif

//--------------------------------------------------------------------------
// openal_error_string()
//
// Returns the human readable error string if there is an error or NULL if not
//
const char* openal_error_string (int get_alc) {
    int i;

    if (get_alc) {
        ALCdevice* device = alcGetContextsDevice (alcGetCurrentContext ());

        i = alcGetError (device);

        if (i != ALC_NO_ERROR) return (const char*)alcGetString (device, i);
    }
    else {
        i = alGetError ();

        if (i != AL_NO_ERROR) return (const char*)alGetString (i);
    }

    return NULL;
}

ALenum openal_get_format (ALint bits, ALint n_channels) {
    ALenum format = AL_INVALID_VALUE;

    if ((n_channels < 1) || (n_channels > 2)) { return format; }

    switch (bits) {
    case 32:
        format = (n_channels == 1) ? AL_FORMAT_MONO_FLOAT32
                                   : AL_FORMAT_STEREO_FLOAT32;
        break;

    case 16:
        format = (n_channels == 1) ? AL_FORMAT_MONO16 : AL_FORMAT_STEREO16;
        break;

    case 8:
        format = (n_channels == 1) ? AL_FORMAT_MONO8 : AL_FORMAT_STEREO8;
        break;
    }

    return format;
}

static bool
openal_device_sort_func (const OALdevice& d1, const OALdevice& d2) {
    if (d1.type > d2.type) { return true; }

    return false;
}

static void find_playback_device (OpenALInformation* info) {
    const auto config_device = fs2::registry::read (
        "Sound.PlaybackDevice", "");

    const auto default_device = info->default_playback_device;

    // in case they are the same, we only want to test it once
    bool same_device = config_device == default_device;

    for (auto& device : info->playback_devices) {
        // TODO: this seems wrong
        OALdevice new_device (device.c_str ());

        if (same_device) {
            if (config_device == device)
                new_device.type = OAL_DEVICE_USER;
        }
        else if (default_device == device) {
            new_device.type = OAL_DEVICE_DEFAULT;
        }

        PlaybackDevices.push_back (new_device);
    }

    if (PlaybackDevices.empty ()) { return; }

    std::sort (
        PlaybackDevices.begin (), PlaybackDevices.end (),
        openal_device_sort_func);

    ALCdevice* device = NULL;
    ALCcontext* context = NULL;

    // for each device that we have available, try and figure out which to use
    for (size_t idx = 0; idx < PlaybackDevices.size (); ++idx) {
        OALdevice* pdev = &PlaybackDevices[idx];

        // open our specfic device
        device = alcOpenDevice ((const ALCchar*)pdev->device_name.c_str ());

        if (device == NULL) { continue; }

        context = alcCreateContext (device, NULL);

        if (context == NULL) {
            alcCloseDevice (device);
            continue;
        }

        alcMakeContextCurrent (context);
        alcGetError (device);

        // check how many sources we can create
        static const int MIN_SOURCES = 48; // MAX_CHANNELS + 16 spare
        int si = 0;

        for (si = 0; si < MIN_SOURCES; si++) {
            ALuint source_id = 0;
            alGenSources (1, &source_id);

            if (alGetError () != AL_NO_ERROR) { break; }

            alDeleteSources (1, &source_id);
        }

        if (si == MIN_SOURCES) {
            // ok, it supports our minimum requirements
            pdev->usable = true;

            // need this for the future
            Playback_device = pdev->device_name;

            // done
            break;
        }
        else {
            // clean up for next pass
            alcMakeContextCurrent (NULL);
            alcDestroyContext (context);
            alcCloseDevice (device);

            context = NULL;
            device = NULL;
        }
    }

    alcMakeContextCurrent (NULL);

    if (context) { alcDestroyContext (context); }

    if (device) { alcCloseDevice (device); }
}

static void find_capture_device (OpenALInformation* info) {
    const auto config_device = fs2::registry::read (
        "Sound.CaptureDevice", "");

    const auto default_device = info->default_capture_device;

    //
    // in case they are the same, we only want to test it once
    //
    const bool same_device = config_device == default_device;

    for (auto& device : info->capture_devices) {
        // TODO: this whole thing seems wrong
        OALdevice new_device (device.c_str ());

        if (!same_device) {
            if (config_device == device)
                new_device.type = OAL_DEVICE_USER;
        }
        else if (default_device == device) {
            new_device.type = OAL_DEVICE_DEFAULT;
        }

        CaptureDevices.push_back (new_device);
    }

    if (CaptureDevices.empty ()) { return; }

    std::sort (
        CaptureDevices.begin (), CaptureDevices.end (),
        openal_device_sort_func);

    // for each device that we have available, try and figure out which to use
    for (size_t idx = 0; idx < CaptureDevices.size (); idx++) {
        const ALCchar* device_name = CaptureDevices[idx].device_name.c_str ();

        ALCdevice* device = alcCaptureOpenDevice (
            device_name, 22050, AL_FORMAT_MONO8, 22050 * 2);

        if (device == NULL) { continue; }

        if (alcGetError (device) != ALC_NO_ERROR) {
            alcCaptureCloseDevice (device);
            continue;
        }

        // ok, we should be good with this one
        Capture_device = CaptureDevices[idx].device_name;

        alcCaptureCloseDevice (device);

        break;
    }
}

// initializes hardware device from perferred/default/enumerated list
bool openal_init_device (std::string* playback, std::string* capture) {
    if (!Playback_device.empty ()) {
        if (playback) { *playback = Playback_device; }

        if (capture) { *capture = Capture_device; }

        return true;
    }

    if (playback) { playback->erase (); }

    if (capture) { capture->erase (); }

    // This reuses the code for the launcher to make sure everything is
    // consistent
    auto platform_info = openal_get_platform_information ();

    if (platform_info.version_major <= 1 && platform_info.version_minor < 1) {
        EE << "OpenAL version < v1.1";
        return false;
    }

    // go through and find out what devices we actually want to use ...
    find_playback_device (&platform_info);
    find_capture_device (&platform_info);

    if (Playback_device.empty ()) { return false; }

#ifndef NDEBUG
    if (!PlaybackDevices.empty ()) {
        II << "available playback devices : ";

        for (size_t idx = 0; idx < PlaybackDevices.size (); idx++) {
            if (PlaybackDevices[idx].type == OAL_DEVICE_USER) {
                II << "  --> >>" <<PlaybackDevices[idx].device_name << "<<";
            }
            else if (PlaybackDevices[idx].type == OAL_DEVICE_DEFAULT) {
                II << "  --> " <<PlaybackDevices[idx].device_name << " *";
            }
        }
    }

    if (!CaptureDevices.empty ()) {
        II << "available capture devices : ";

        for (size_t idx = 0; idx < CaptureDevices.size (); idx++) {
            if (CaptureDevices[idx].type == OAL_DEVICE_USER) {
                II << "  --> >>" << CaptureDevices[idx].device_name << "<<";
            }
            else if (CaptureDevices[idx].type == OAL_DEVICE_DEFAULT) {
                II << "  --> " << CaptureDevices[idx].device_name << "*";
            }
        }
    }
#endif

    // cleanup
    PlaybackDevices.clear ();
    CaptureDevices.clear ();

    if (playback) { *playback = Playback_device; }

    if (capture) { *capture = Capture_device; }

    return true;
}

static void get_version_info (OpenALInformation* info) {
    // initialize default setup first, for version check...
    ALCdevice* device = alcOpenDevice (NULL);

    if (device == NULL) { return; }

    ALCcontext* context = alcCreateContext (device, NULL);

    if (context == NULL) {
        alcCloseDevice (device);
        return;
    }

    alcMakeContextCurrent (context);

    // version check (for 1.0 or 1.1)
    ALCint AL_minor_version = 0;
    ALCint AL_major_version = 0;
    alcGetIntegerv (
        NULL, ALC_MAJOR_VERSION, sizeof (ALCint), &AL_major_version);
    alcGetIntegerv (
        NULL, ALC_MINOR_VERSION, sizeof (ALCint), &AL_minor_version);

    info->version_major = static_cast< uint32_t > (AL_major_version);
    info->version_minor = static_cast< uint32_t > (AL_minor_version);

    alcGetError (device);

    // close default device
    alcMakeContextCurrent (NULL);
    alcDestroyContext (context);
    alcCloseDevice (device);
}

static bool device_supports_efx (const char* device_name) {
    ALCdevice* device = alcOpenDevice (device_name);

    if (device == NULL) { return false; }

    bool result = false;
    if (alcIsExtensionPresent (device, (const ALchar*)"ALC_EXT_EFX") ==
        AL_TRUE) {
        result = true;
    }

    alcCloseDevice (device);
    return result;
}

static void enumerate_playback_devices (OpenALInformation* info) {
    info->playback_devices.clear ();
    info->default_playback_device.clear ();

    auto default_device = alcGetString (NULL, ALC_DEFAULT_DEVICE_SPECIFIER);
    if (default_device != nullptr) {
        info->default_playback_device = default_device;
        info->efx_support.push_back (std::make_pair (
            std::string (default_device),
            device_supports_efx (default_device)));
    }

    if (alcIsExtensionPresent (NULL, (const ALCchar*)"ALC_ENUMERATION_EXT") ==
        AL_TRUE) {
        const char* all_devices = NULL;

        if (alcIsExtensionPresent (
                NULL, (const ALCchar*)"ALC_ENUMERATE_ALL_EXT") == AL_TRUE) {
            all_devices =
                (const char*)alcGetString (NULL, ALC_ALL_DEVICES_SPECIFIER);
        }
        else {
            all_devices =
                (const char*)alcGetString (NULL, ALC_DEVICE_SPECIFIER);
        }

        const char* str_list = all_devices;
        size_t ext_length = 0;

        if ((str_list != NULL) && ((ext_length = strlen (str_list)) > 0)) {
            while (ext_length) {
                info->playback_devices.push_back (std::string (str_list));
                info->efx_support.push_back (std::make_pair (
                    std::string (str_list), device_supports_efx (str_list)));

                str_list += (ext_length + 1);
                ext_length = strlen (str_list);
            }
        }
    }
    else {
        if (default_device) {
            info->playback_devices.push_back (std::string (default_device));
        }
    }
}

static void enumerate_capture_devices (OpenALInformation* info) {
    info->capture_devices.clear ();
    info->default_capture_device.clear ();

    auto default_device =
        alcGetString (NULL, ALC_CAPTURE_DEFAULT_DEVICE_SPECIFIER);
    if (default_device != nullptr) {
        info->default_capture_device = default_device;
    }

    if (alcIsExtensionPresent (NULL, (const ALCchar*)"ALC_ENUMERATION_EXT") ==
        AL_TRUE) {
        const char* all_devices =
            (const char*)alcGetString (NULL, ALC_CAPTURE_DEVICE_SPECIFIER);

        const char* str_list = all_devices;
        size_t ext_length = 0;

        if ((str_list != NULL) && ((ext_length = strlen (str_list)) > 0)) {
            while (ext_length) {
                info->capture_devices.push_back (std::string (str_list));

                str_list += (ext_length + 1);
                ext_length = strlen (str_list);
            }
        }
    }
    else {
        if (default_device) {
            info->capture_devices.push_back (std::string (default_device));
        }
    }
}

OpenALInformation openal_get_platform_information () {
    OpenALInformation info;

    get_version_info (&info);

    enumerate_playback_devices (&info);
    enumerate_capture_devices (&info);

    return info;
}
