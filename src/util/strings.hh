// -*- mode: c++; -*-

#ifndef FREESPACE2_UTIL_STRINGS_HH
#define FREESPACE2_UTIL_STRINGS_HH

#include <defs.hh>

#include <ctype.h>

inline void
stolower (char* s) {
    if (s && s [0]) {
        for (; s [0]; ++s) {
            s [0] = tolower (s [0]);
        }
    }
}

#endif // FREESPACE2_UTIL_STRINGS_HH
