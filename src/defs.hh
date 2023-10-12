// -*- mode: c++; -*-

#ifndef FREESPACE2_DEFS_HH
#define FREESPACE2_DEFS_HH

#define BOOST_LOG_DYN_LINK 1

#include "config.hh"
#include <assert/assert.hh>
#include <log/log.hh>

#ifdef NDEBUG
#  define BOOST_DISABLE_ASSERTS
#endif // NDEBUG

#include <boost/assert.hpp>
#define ASSERT BOOST_ASSERT

#define FS_VERSION_MAJOR 3
#define FS_VERSION_MINOR 8
#define FS_VERSION_BUILD 1

#define FS_VERSION_HAS_REVISION 0

#define FS_VERSION_REVISION 0
#define FS_VERSION_REVISION_STRING "0"

#define FS_VERSION_FULL "3.8.1"
#define FS_PRODUCT_VERSION "3.8.1"

#define FS2_DO_PASTE(a, b) a##b
#define FS2_PASTE(a, b) FS2_DO_PASTE (a, b)

#define FS2_UNUSED(x) ((void)x)

#if !defined (FS2_NO_VA_COPY)
#  define FS2_VA_COPY(a,b) va_copy (a,b)
#elif !defined (FS2_NO__VA_COPY)
#  define FS2_VA_COPY(a,b) __va_copy (a,b)
#else
#  define FS2_VA_COPY(a,b) (a) = (b)
#endif // HAVE_*VA_COPY

#if defined (FS2_NO_POSIX_VSNPRINTF)
#  define FS2_VSNFMT_MAX  (1024U * 1024U)
#else
#  define FS2_VSNFMT_MAX  (size_t(-1) - 1)
#endif // FS2_VSNPRINTF_OVERFLOW_NEGATIVE

#define LOCATION __FILE__, __LINE__

//
// Legacy
//
#ifndef MIN
#  define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif

#ifndef MAX
#  define MAX(a, b) (((a) > (b)) ? (a) : (b))
#endif

// Some constants for stuff
#define MAX_FILENAME_LEN 32 // Length for filenames, ie "title.pcx"
#define MAX_PATH_LEN 256    // Length for pathnames, ie "c:\bitmaps\title.pcx"

#define TRUE 1
#define FALSE 0

#define INVALID_SIZE  (size_t (-1))

#define UNINITIALIZED 0x7f8e6d9c
#define MAX_PLAYERS 12

#define PI   3.141592654f

#define PI2  (PI * 2.0f)
#define PI_2 (PI / 2.0f)

#define RAND_MAX_2  (RAND_MAX / 2)
#define RAND_MAX_1f (1.0f / RAND_MAX)

#define NOX(s) s

//-----Cutscene stuff
#define CUB_NONE           0   // No bars
#define CUB_CUTSCENE (1 << 0)  // List of types of bars
#define CUB_GRADUAL  (1 << 15) // Styles to get to bars

#define GM_MULTIPLAYER       (1 << 0)

#define SINGLEPLAYER !(Game_mode & GM_MULTIPLAYER)

#define GM_NORMAL            (1 << 1)
#define GM_DEAD_DIED         (1 << 2) // Died, waiting to blow up.
#define GM_DEAD_BLEW_UP      (1 << 3) // Blew up.
#define GM_DEAD_ABORTED      (1 << 4) // Abort dead sequence.
#define GM_IN_MISSION        (1 << 5) // Player is in mission

#define GM_DEAD (GM_DEAD_DIED | GM_DEAD_BLEW_UP | GM_DEAD_ABORTED)

#define GM_STANDALONE_SERVER (1 << 8)
#define GM_STATS_TRANSFER    (1 << 9)  // in the process of stats transfer
#define GM_CAMPAIGN_MODE     (1 << 10) // are we currently in a campaign.

#define VM_EXTERNAL          (1 << 0) // Set if not viewing from player position.
#define VM_TRACK             (1 << 1) // Set if viewer is tracking target.
#define VM_DEAD_VIEW         (1 << 2) // Set if viewer is watching from dead view.
#define VM_CHASE             (1 << 3) // Chase view.
#define VM_OTHER_SHIP        (1 << 4) // View from another ship.
#define VM_CAMERA_LOCKED     (1 << 5) // Set if player does not have control of the camera
#define VM_WARP_CHASE        (1 << 6) // View while warping out (form normal view mode)
#define VM_PADLOCK_UP        (1 << 7)
#define VM_PADLOCK_REAR      (1 << 8)
#define VM_PADLOCK_LEFT      (1 << 9)
#define VM_PADLOCK_RIGHT     (1 << 10)
#define VM_WARPIN_ANCHOR     (1 << 11) // special warpin camera mode
#define VM_TOPDOWN           (1 << 12) // Camera is looking down on ship
#define VM_FREECAMERA        (1 << 13) // Camera is not attached to any particular object, probably
                                       // under SEXP control
#define VM_CENTERING         (1 << 14) // View is springing to center

#define VM_PADLOCK_ANY \
    (VM_PADLOCK_UP | VM_PADLOCK_REAR | VM_PADLOCK_LEFT | VM_PADLOCK_RIGHT)

#define MAX_PLAYERS 12

// parselo.h
#define PATHNAME_LENGTH 192
#define NAME_LENGTH 32
#define SEXP_LENGTH 128
#define DATE_LENGTH 32
#define TIME_LENGTH 16
#define DATE_TIME_LENGTH 48
#define NOTES_LENGTH 1024
#define MULTITEXT_LENGTH 4096
#define FILESPEC_LENGTH 64
#define MESSAGE_LENGTH 512
#define TRAINING_MESSAGE_LENGTH 512
#define CONDITION_LENGTH 64

// missionparse.h
#define MISSION_DESC_LENGTH 512

// player.h
#define CALLSIGN_LEN 28
#define SHORT_CALLSIGN_PIXEL_W 80

#define MAX_IFFS 10

// ship.h
#define MAX_SHIPS 400
#define SHIPS_LIMIT 400

// missionparse.h and then redefined to the same value in sexp.h
#define TOKEN_LENGTH 32

#define MAX_SHIP_CLASSES_MULTI 130
#define MAX_SHIP_CLASSES 500
#define MAX_WINGS 75
#define MAX_SHIPS_PER_WING 6
#define MAX_STARTING_WINGS 3 // number of wings player can start a mission with
#define MAX_SQUADRON_WINGS 5 // number of wings in squadron (displayed on HUD)

#define MAX_TVT_TEAMS 2          // number of teams in a TVT game
#define MAX_TVT_WINGS_PER_TEAM 1 // number of wings per team in a TVT game
#define MAX_TVT_WINGS (MAX_TVT_TEAMS* MAX_TVT_WINGS_PER_TEAM) // number of wings in a TVT game

// model.h
#define MAX_SHIP_PRIMARY_BANKS 3
#define MAX_SHIP_SECONDARY_BANKS 4
#define MAX_SHIP_WEAPONS (MAX_SHIP_PRIMARY_BANKS + MAX_SHIP_SECONDARY_BANKS)

#define MAX_COMPLETE_ESCORT_LIST 20

// weapon.h
#define MAX_WEAPONS 2000

#define MAX_WEAPON_TYPES 300

// model.h
#define MAX_MODEL_TEXTURES 64
#define MAX_POLYGON_MODELS 300

// object.h
#define MAX_OBJECTS 3500

// weapon.h (and beam.h)
#define MAX_BEAM_SECTIONS 5

#define DETAIL_DEFAULT (0xFFFFFFFF)

#define DETAIL_FLAG_STARS (1 << 0)   // draw the stars
#define DETAIL_FLAG_NEBULAS (1 << 1) // draw the motion debris
#define DETAIL_FLAG_MOTION (1 << 2)  // draw the motion debris
#define DETAIL_FLAG_PLANETS (1 << 3) // draw planets
#define DETAIL_FLAG_MODELS (1 << 4)  // draw models not as blobs
#define DETAIL_FLAG_LASERS (1 << 5)  // draw lasers not as pixels
#define DETAIL_FLAG_CLEAR (1 << 6) // clear screen background after each frame
#define DETAIL_FLAG_HUD (1 << 7)   // draw hud stuff
#define DETAIL_FLAG_FIREBALLS (1 << 8) // draw fireballs
#define DETAIL_FLAG_COLLISION (1 << 9) // use good collision detection

#define NOISE_NUM_FRAMES 15

#include <shared/types.hh>
#include <shared/globals.hh>

#endif // FREESPACE2_DEFS_HH
