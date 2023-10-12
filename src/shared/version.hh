// -*- mode: c++; -*-

#ifndef FREESPACE2_SHARED_VERSION_HH
#define FREESPACE2_SHARED_VERSION_HH

#include <defs.hh>

//
// VERSION DEFINES/VARS
//

// Here are the version defines.
// Gets displayed as MAJOR.MINOR, or 1.21 if MAJOR = 1, MINOR = 21.
// Prior to release, MAJOR should be zero.  After release, it should be 1.
// Probably never increase to 2 as that could   cause confusion with a sequel.
// MINOR should increase by 1 for each minor upgrade and by 10 for major
// upgrades.    With each rev we send, we should increase MINOR.
// Version history (full version):
// 1.0     Initial US/UK release
// 1.01    Patch for Win95 volume label bug
// 1.20    German release version

// fs2_open version numbers:
// the first version is 3.0 :-)
// Major.Minor.Bugfix

// The GCC POSIX headers seem to think defining a common word like "major" is a
// good idea... FYI, no it isn't.
#ifdef major
#undef major
#endif
#ifdef minor
#undef minor
#endif

namespace gameversion {

struct version {
    int major = 0;
    int minor = 0;
    int build = 0;
    int revision = 0;

    version () {}

    version (int major, int minor, int build, int revision);

    bool operator< (const version& other) const;
    bool operator== (const version& other) const;
    bool operator!= (const version& other) const;
    bool operator> (const version& rhs) const;
    bool operator<= (const version& rhs) const;
    bool operator>= (const version& rhs) const;
};

version parse_version ();

version get_executable_version ();

/**
 * @brief Checks if the current version is at least the given version
 *
 * @param v The version to check
 *
 * @returns @c true when we are at least the given version, @c false otherwise
 */
bool check_at_least (const version& v);

/**
 * @brief Returns the string representation of the passed version
 * @param major The version to format
 * @returns A string representation of the version number
 */
std::string format_version (const version& v);

} // namespace gameversion

#endif // FREESPACE2_SHARED_VERSION_HH
