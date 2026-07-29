/*
 * Copyright (C) Volition, Inc. 1999.  All rights reserved.
 *
 * All source code herein is the property of Volition, Inc. You may not sell 
 * or otherwise commercially exploit the source or things you created based on the 
 * source.
 *
*/

#ifndef _PSTYPES_H
#define _PSTYPES_H

// Build defines.  The retail demo/OEM/E3/press-tour/PD/beta configurations
// and the RELEASE_REAL knob were collapsed out 2026-07-29 -- the shipping
// retail configuration is the only build.  Localization remains a knob:
// #define GERMAN_BUILD          // build for German (this is now used)

// uncomment this #define for DVD version (makes popups say DVD instead of CD 2 or whatever): JCF 5/10/2000
// #define DVD_MESSAGE_HACK

#define GAME_CD_CHECK

#include <ctype.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h> // For NULL, etc
#include <stdlib.h>
#include <string.h>
#include <strings.h> // strcasecmp
#include <unistd.h>

// MSVC runtime spellings used throughout the retail sources
#define stricmp strcasecmp
#define strnicmp strncasecmp
#define _isnan isnan
#define _hypot hypot
#define _cdecl
#define __cdecl
#define _MAX_PATH 260
#define _MAX_FNAME 256
#define _unlink unlink

// MSVC path splitter; any output pointer may be NULL
void _splitpath(const char *path, char *drive, char *dir, char *fname, char *ext);

// Win32 MulDiv: 64-bit intermediate, rounds to nearest (half away from zero)
inline int
MulDiv(int number, int numerator, int denominator)
{
    if (denominator == 0)
        return -1;
    long long t = (long long)number * numerator;
    long long half = denominator > 0 ? denominator / 2
                                     : -(long long)denominator / 2;
    return (int)((t >= 0 ? t + half : t - half) / denominator);
}

inline char *
strlwr(char *str)
{
    for (char *p = str; *p; p++)
        *p = (char)tolower((unsigned char)*p);
    return str;
}
#define _strlwr strlwr
#define _strnicmp strncasecmp

// MSVC itoa; radix 10 and 16 cover every call site
inline char *
itoa(int value, char *str, int radix)
{
    sprintf(str, radix == 16 ? "%x" : "%d", value);
    return str;
}

// _filelength(fileno(fp)) idiom from the MSVC runtime
long filelength(int fd);

// value to represent an uninitialized state in any int or uint
#define UNINITIALIZED 0x7f8e6d9c

#define MAX_PLAYERS 12

#define MAX_TEAMS 3

#define STRUCT_CMP(a, b) memcmp((void *)&a, (void *)&b, sizeof(a))

// make module local varilable static.
#define LOCAL static

typedef long long longlong;
typedef long fix;
typedef unsigned char ubyte;
typedef unsigned short ushort;
typedef unsigned int uint;

// 4-byte file/chunk magics are read from disk as a little-endian int and
// compared against these constants; spell the id as the byte sequence that
// appears in the file (retail spelled them as reversed multichar literals,
// whose type is int -- returning int keeps the comparisons' signedness)
constexpr int fourcc(const char (&s)[5])
{
    return int(uint(ubyte(s[0])) | uint(ubyte(s[1])) << 8 |
               uint(ubyte(s[2])) << 16 | uint(ubyte(s[3])) << 24);
}

// HARDWARE_ONLY undefined: this build IS the software renderer

//Stucture to store clipping codes in a word
typedef struct ccodes
{
    ubyte cc_or,
        cc_and; //or is low byte, and is high byte ("or"/"and" are C++ keywords)
} ccodes;

typedef struct vector
{
    union
    {
        struct
        {
            float x, y, z;
        };
        float a1d[3];
    };
} vector;

// A vector referenced as an array
typedef struct vectora
{
    float xyz[3];
} vectora;

typedef struct vec2d
{
    float i, j;
} vec2d;

// Used for some 2d primitives, like gr_poly
typedef struct vert2df
{
    float x, y;
} vert2df;

typedef struct angles
{
    float p, b, h;
} angles;

typedef struct matrix
{
    union
    {
        struct
        {
            vector rvec, uvec, fvec;
        };
        float a2d[3][3];
        float a1d[9];
    };
} matrix;

typedef struct uv_pair
{
    float u, v;
} uv_pair;

// Used to store rotated points for mines.
// Has flag to indicate if projected.
typedef struct vertex
{
    float x, y, z; // world space position
    float sx, sy, sw; // screen space position (sw == 1/z)
    float u, v; // texture position
    ubyte r, g, b, a; // color.  Use b for darkening;
    ubyte codes; // what sides of view pyramid this point is on/off.  0 = Inside view pyramid.
    ubyte flags; // Projection flags.  Indicates whether it is projected or not or if projection overflowed.
    ubyte pad[2]; // pad structure to be 4 byte aligned.
} vertex;

// antialiased bitmap
#define BMP_AABITMAP (1 << 0)
// transparent texture
#define BMP_TEX_XPARENT (1 << 1)
// nondarkening texture
#define BMP_TEX_NONDARK (1 << 2)
// so we can identify all "normal" textures
#define BMP_TEX_OTHER (1 << 3)

// any texture type
#define BMP_TEX_ANY (BMP_TEX_XPARENT | BMP_TEX_NONDARK | BMP_TEX_OTHER)

// max res == 1024x768. max texture size == 256
#define MAX_BMAP_SECTIONS_X 4
#define MAX_BMAP_SECTIONS_Y 3
#define MAX_BMAP_SECTION_SIZE 256
typedef struct bitmap_section_info
{
    ushort sx[MAX_BMAP_SECTIONS_X]; // x offset of each section
    ushort sy[MAX_BMAP_SECTIONS_Y]; // y offset of each section

    ubyte num_x, num_y; // number of x and y sections
} bitmap_section_info;

typedef struct bitmap
{
    short w, h; // Width and height
    short rowsize; // What you need to add to go to next row
    ubyte bpp; // How many bits per pixel it is. (7,8,15,16,24,32)
    ubyte flags; // See the BMP_???? defines for values
    uintptr_t
        data; // Pointer to data, or maybe offset into VRAM.  (was uint; pointers no longer fit in 32 bits)
    ubyte *
        palette; // If bpp==8, this is pointer to palette.   If the BMP_NO_PALETTE_MAP flag
    // is not set, this palette just points to the screen palette. (gr_palette)

    bitmap_section_info sections;
} bitmap;

//These are defined in globalincs/debug.cpp
extern void WinAssert(const char *text, const char *filename, int line);
extern void Error(const char *filename, int line, const char *format, ...);
extern void Warning(const char *filename, int line, const char *format, ...);

#include <osapi/outwnd.hh>

// To debug printf do this:
// mprintf(( "Error opening %s\n", filename ));
#ifndef NDEBUG
#define mprintf(args) outwnd_printf2 args
#define nprintf(args) outwnd_printf args
#else
#define mprintf(args)
#define nprintf(args)
#endif

#define LOCATION __FILE__, __LINE__

// To flag an error, you can do this:
// Error( __FILE__, __LINE__, "Error opening %s", filename );
// or,
// Error( LOCATION, "Error opening %s", filename );

#if defined(NDEBUG)
#define Assert(x)                                                                \
    do {                                                                         \
    } while (0)
#else
void gr_activate(int);
#define Assert(x)                                                                \
    do {                                                                         \
        if (!(x)) {                                                              \
            gr_activate(0);                                                      \
            WinAssert(#x, __FILE__, __LINE__);                                   \
            gr_activate(1);                                                      \
        }                                                                        \
    } while (0)
#endif

//#define Int3() _asm { int 3 }

#ifdef INTERPLAYQA
// Interplay QA version of Int3
#define Int3()                                                                   \
    do {                                                                         \
    } while (0)

// define to call from Warning function above since it calls Int3, so without this, we
// get put into infinite dialog boxes
#define AsmInt3() _asm { int 3 }

#else
#if defined(NDEBUG)
// No debug version of Int3
#define Int3()                                                                   \
    do {                                                                         \
    } while (0)
#else
void debug_int3();

// Debug version of Int3
#define Int3() debug_int3()
#endif // NDEBUG && DEMO
#endif // INTERPLAYQA

// min/max were macros; as templates they no longer poison standard headers.
template< class A, class B >
constexpr auto
min(A a, B b)
{
    return (b < a) ? b : a;
}
template< class A, class B >
constexpr auto
max(A a, B b)
{
    return (a < b) ? b : a;
}

#define PI 3.141592654f
// PI*2
#define PI2 (3.141592654f * 2.0f)
#define ANG_TO_RAD(x) ((x) * PI / 180)


// Debug console stuff

// Here is a a sample command to toggle something that would
// be called by doing "toggle it" from the debug console command window/

/*
DCF(toggle_it,"description")
{
   if (Dc_command)   {
      This_var = !This_var;
   }

   if (Dc_help)   {
      dc_printf( "Usage: sample\nToggles This_var on/off.\n" );
   }

   if (Dc_status) {
      dc_printf( "The status is %d.\n", This_var );
   }
*/

class debug_command
{
public:
    const char *name;
    const char *help;
    void (*func)();
    debug_command(const char *name, const char *help,
                  void (*func)()); // constructor
};

#define DCF(function_name, help_text)                                            \
    void dcf_##function_name();                                                  \
    debug_command dc_##function_name(#function_name, help_text,                  \
                                     dcf_##function_name);                       \
    void dcf_##function_name()

// Starts the debug console
extern void debug_console(void (*func)() = NULL);

// The next three variables tell your function what to do.  It should
// only change something if the dc_command is set.   A minimal function
// needs to process the dc_command.   Usually, these will be called in
// these combinations:
// dc_command=true, dc_status=true  means process it and show status
// dc_help=true means show help only
// dc_status=true means show status only
// I would recommend doing this in each function:
// if (dc_command) { process command }
// if (dc_help) { print out help }
// if (dc_status) { print out status }
// with the last two being optional

extern int Dc_command; // If this is set, then process the command
extern int
    Dc_help; // If this is set, then print out the help text in the form, "usage: ... \nLong description\n" );
extern int
    Dc_status; // If this is set, then print out the current status of the command.

void dc_get_arg(
    uint flags); // Gets the next argument.   If it doesn't match the flags, this function will print an error and not return.
extern char *
    Dc_arg; // The (lowercased) string value of the argument retrieved from dc_arg
extern char *Dc_arg_org; // Dc_arg before it got converted to lowercase
extern uint Dc_arg_type; // The type of dc_arg.
extern char *
    Dc_command_line; // The rest of the command line, from the end of the last processed arg on.
extern int
    Dc_arg_int; // If Dc_arg_type & ARG_INT or ARG_HEX is set, then this is the value
extern float
    Dc_arg_float; // If Dc_arg_type & ARG_FLOAT is set, then this is the value

// Outputs text to the console
void dc_printf(const char *format, ...);

// Each dc_arg_type can have one or more of these flags set.
// This is because some things can fit into two catagories.
// Like 1 can be an integer, a float, a string, or a true boolean
// value.
// no argument
#define ARG_NONE (1 << 0)
// Anything.
#define ARG_ANY 0xFFFFFFFF
// any valid string
#define ARG_STRING (1 << 1)
// a quoted string
#define ARG_QUOTE (1 << 2)
// a valid integer
#define ARG_INT (1 << 3)
// a valid floating point number
#define ARG_FLOAT (1 << 4)

// some specific commonly used predefined types. Can add up to (1<<31)
#define ARG_HEX                                                                  \
    (1                                                                           \
     << 5) // a valid hexadecimal integer. Note that ARG_INT will always be set also in this case.
// on, true, non-zero number
#define ARG_TRUE (1 << 6)
// off, false, zero
#define ARG_FALSE (1 << 7)
// Plus sign
#define ARG_PLUS (1 << 8)
// Minus sign
#define ARG_MINUS (1 << 9)
// a comma
#define ARG_COMMA (1 << 10)

// A shortcut for boolean only variables.
// Example:
// DCF_BOOL( lighting, Show_lighting )
//
#define DCF_BOOL(function_name, bool_variable)                                                        \
    void dcf_##function_name();                                                                       \
    debug_command dc_##function_name(#function_name, "Toggles " #bool_variable,                       \
                                     dcf_##function_name);                                            \
    void dcf_##function_name()                                                                        \
    {                                                                                                 \
        if (Dc_command) {                                                                             \
            dc_get_arg(ARG_TRUE | ARG_FALSE | ARG_NONE);                                              \
            if (Dc_arg_type & ARG_TRUE)                                                               \
                bool_variable = 1;                                                                    \
            else if (Dc_arg_type & ARG_FALSE)                                                         \
                bool_variable = 0;                                                                    \
            else if (Dc_arg_type & ARG_NONE)                                                          \
                bool_variable ^= 1;                                                                   \
        }                                                                                             \
        if (Dc_help)                                                                                  \
            dc_printf(                                                                                \
                "Usage: %s [bool]\nSets %s to true or false.  If nothing passed, then toggles it.\n", \
                #function_name, #bool_variable);                                                      \
        if (Dc_status)                                                                                \
            dc_printf("%s is %s\n", #function_name,                                                   \
                      (bool_variable ? "TRUE" : "FALSE"));                                            \
    }


#include <math/fix.hh>
#include <math/floating.hh>

// Some constants for stuff
// Length for filenames, ie "title.pcx"
#define MAX_FILENAME_LEN 32
// Length for pathnames, ie "c:\bitmaps\title.pcx"
#define MAX_PATH_LEN 128

// contants and defined for byteswapping routines (useful for mac)

#define SWAPSHORT(x) (((ubyte)x << 8) | (((ushort)x) >> 8))

#define SWAPINT(x)                                                               \
    ((x << 24) | (((ulong)x) >> 24) | ((x & 0x0000ff00) << 8) |                  \
     ((x & 0x00ff0000) >> 8))

#ifndef MACINTOSH
#define INTEL_INT(x) x
#define INTEL_SHORT(x) x
#else
#define INTEL_INT(x) SWAPINT(x)
#define INTEL_SHORT(x) SWAPSHORT(x)
#endif

#define TRUE 1
#define FALSE 0

int myrand();

// Callback Loading function.
// If you pass a function to this, that function will get called
// around 10x per second, so you can update the screen.
// Pass NULL to turn it off.
// Call this with the name of a function.  That function will
// then get called around 10x per second.  The callback function
// gets passed a 'count' which is how many times game_busy has
// been called since the callback was set.   It gets called
// one last time with count=-1 when you turn off the callback
// by calling game_busy_callback(NULL).   Game_busy_callback
// returns the current count, so you can tell how many times
// game_busy got called.
// If delta_step is above 0, then it will also make sure it
// calls the callback each time count steps 'delta_step' even
// if 1/10th of a second hasn't elapsed.
extern int game_busy_callback(void (*callback)(int count), int delta_step = -1);

// Call whenever loading to display cursor
extern void game_busy();

// Functions to monitor performance
#ifndef NDEBUG

class monitor
{
public:
    const char *name;
    int value; // Value that gets cleared to 0 each frame.
    int min, max, sum,
        cnt; // Min & Max of value.  Sum is used to calculate average
    monitor(const char *name); // constructor
};

// Creates a monitor variable
#define MONITOR(function_name) monitor mon_##function_name(#function_name)

// Increments a monitor variable
#define MONITOR_INC(function_name, inc)                                          \
    do {                                                                         \
        mon_##function_name.value += (inc);                                      \
    } while (0)

// Call this once per frame to update monitor file
void monitor_update();

#else

#define MONITOR(function_name)

#define MONITOR_INC(function_name, inc)                                          \
    do {                                                                         \
    } while (0)

// Call this once per frame to update monitor file
#define monitor_update()                                                         \
    do {                                                                         \
    } while (0)

#endif

#define NOX(s) s

char *XSTR(const char *str, int index);

// Caps V between MN and MX.
template< class T >
void
CAP(T &v, T mn, T mx)
{
    if (v < mn) {
        v = mn;
    }
    else if (v > mx) {
        v = mx;
    }
}

// stamp checksum stuff

// here is the define for the stamp for this set of code
#define STAMP_STRING                                                             \
    "\001\001\001\001\002\002\002\002Read the Foundation Novels from Asimov.  I liked them."
#define STAMP_STRING_LENGTH 80
#define DEFAULT_CHECKSUM_STRING "\001\001\001\001"
#define DEFAULT_TIME_STRING "\002\002\002\002"

// macro to calculate the checksum for the stamp.  Put here so that we can use different methods
// for different applications.  Requires the local variable 'checksum' to be defined!!!
#define CALCULATE_STAMP_CHECKSUM()                                               \
    do {                                                                         \
        int i, found;                                                            \
                                                                                 \
        checksum = 0;                                                            \
        for (i = 0; i < (int)strlen(ptr); i++) {                                 \
            found = 0;                                                           \
            checksum += ptr[i];                                                  \
            if (checksum & 0x01)                                                 \
                found = 1;                                                       \
            checksum = checksum >> 1;                                            \
            if (found)                                                           \
                checksum |= 0x80000000;                                          \
        }                                                                        \
        checksum |= 0x80000000;                                                  \
    } while (0);

// Memory management functions

// Thin wrappers over the C heap; the Win32 tracking heap and the
// malloc/free/strdup macro redefinitions are gone.

// Returns 0 if not enough RAM.
int vm_init(int min_heap_size);

// Allocates some RAM.
void *vm_malloc(int size);

// Frees some RAM.
void vm_free(void *ptr);

#endif // PS_TYPES_H
