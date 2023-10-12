// -*- mode: c++; -*-

#ifndef FREESPACE2_SHARED_TYPES_HH
#define FREESPACE2_SHARED_TYPES_HH

#include <defs.hh>

#include <cstdio>
#include <cstdlib>
#include <memory.h>
#include <cstring>
#include <algorithm>
#include <cstdint>

#include <SDL.h>

typedef std::int32_t _fs_time_t; // time_t here is 64-bit and we need 32-bit
typedef std::int32_t fix;

typedef ptrdiff_t ptr_s;
typedef size_t ptr_u;

typedef std::int64_t longlong;
typedef std::uint64_t ulonglong;
typedef std::uint8_t ubyte;
typedef std::uint16_t ushort;
typedef std::uint32_t uint;
typedef unsigned long ulong;

// Stucture to store clipping codes in a word
struct ccodes  {
    ubyte cc_or, cc_and; // or is low byte, and is high byte
};

struct vertex;

struct vec4 {
    union {
        struct { float x, y, z, w; } xyzw;
        float a1d[4];
    };
};

struct bvec4 { bool x, y, z, w; };

// sometimes, you just need some integers
struct ivec3 { int x, y, z; };
struct ivec2 { int x, y; };

struct vec3d {
    union {
        struct { float x, y, z; } xyz;
        float a1d[3];
    };
};

struct vec2d { float x, y; };
struct angles_t { float p, b, h; };

struct matrix {
    union {
        struct { vec3d rvec, uvec, fvec; } vec;
        float a2d[3][3];
        float a1d[9];
    };
};

struct matrix4 {
    union {
        struct { vec4 rvec, uvec, fvec, pos; } vec;
        float a2d[4][4];
        float a1d[16];
    };
};

struct uv_pair { float u, v; };

/** Compares two uv_pairs */
inline bool operator== (const uv_pair& left, const uv_pair& right) {
    return (left.u == right.u) && (left.v == right.v);
}

/**
  * Represents a point in 3d screen space. 'w' is 1/z.
  *  Like vec3d but for screens.
  * Note: this is a struct, not a class, so no member functions.
  */
struct screen3d  {
    union {
        struct { float x, y, w; } xyw;
        float a1d[3];
    };
};

/** Compares two screen3ds */
inline bool operator== (const screen3d& self, const screen3d& other) {
    return (
        self.xyw.x == other.xyw.x && self.xyw.y == other.xyw.y &&
        self.xyw.w == other.xyw.w);
}

/** Used to store rotated points for mines. Has flag to indicate if projected.

Note: this is a struct, not a class, so no memeber functions. */
struct vertex  {
    vec3d world;              // world space position
    screen3d screen;          // screen space position (sw == 1/z)
    uv_pair texture_position; // texture position
    ubyte r, g, b, a;         // color.  Use b for darkening;
    ubyte codes; // what sides of view pyramid this point is on/off.  0 =
                 // Inside view pyramid.
    ubyte flags; // Projection flags.  Indicates whether it is projected or not
                 // or if projection overflowed.
    ubyte pad[2]; // pad structure to be 4 byte aligned.
};

struct effect_vertex  {
    vec3d position;
    uv_pair tex_coord;
    float radius;
    ubyte r, g, b, a;
};

struct particle_pnt {
    vec3d position;
    float size;
    vec3d up;
};

// def_list
struct flag_def_list {
    const char* name;
    int def;
    ubyte var;
};

template< class T >
struct flag_def_list_new {
    const char* name; // The parseable representation of this flag
    T def;            // The flag definition for this flag
    bool in_use; // Whether or not this flag is currently in use or obsolete
    bool is_special; // Whether this flag requires special processing. See
                     // parse_string_flag_list<T, T> for details
};

// weapon count list (mainly for pilot files)
struct wep_t  {
    int index;
    int count;
};

struct coord2d  {
    int x, y;
};

int myrand ();
int rand32 (); // returns a random number between 0 and 0x7fffffff

// lod checker for (modular) table parsing
struct lod_checker  {
    char filename[MAX_FILENAME_LEN];
    int num_lods;
    int override;
};

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
extern int
game_busy_callback (void (*callback) (int count), int delta_step = -1);

// Call whenever loading to display cursor
extern void game_busy (const char* filename = NULL);

const char* XSTR (const char* str, int index);

// Caps V between MN and MX.
template< class T >
void CAP (T& v, T mn, T mx) {
    if (v < mn) { v = mn; }
    else if (v > mx) {
        v = mx;
    }
}

// faster version of CAP()
#define CLAMP(x, min, max)    \
    do {                      \
        if ((x) < (min))      \
            (x) = (min);      \
        else if ((x) > (max)) \
            (x) = (max);      \
    } while (false)

class camid {
private:
    int sig;
    size_t idx;

public:
    camid ();
    camid (size_t n_idx, int n_sig);

    class camera* getCamera ();
    size_t getIndex ();
    int getSignature ();
    bool isValid ();
};

// check to see that a passed string is valid, ie:
// - has >0 length
// - is not "none"
// - is not "<none>"
inline bool VALID_FNAME (const char* x) {
    return strlen ((x)) && strcasecmp ((x), "none") != 0 &&
           strcasecmp ((x), "<none>") != 0;
}
/**
 * @brief Checks if the specified string may be a valid file name
 *
 * @warning This only does a quick check against an empty string and a few
 * known invalid names. It does not check if the file actually exists.
 *
 * @param x The file name to check
 * @return @c true if the name is valid, @c false otherwise
 */
inline bool VALID_FNAME (const std::string& x) {
    if (x.empty ()) { return false; }
    if (!strcasecmp (x.c_str (), "none")) { return false; }
    if (!strcasecmp (x.c_str (), "<none>")) { return false; }
    return true;
}

#endif // FREESPACE2_SHARED_TYPES_HH
