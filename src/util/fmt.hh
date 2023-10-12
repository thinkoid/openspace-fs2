// -*- mode: c++; -*-

#ifndef FREESPACE2_UTIL_FMT_HH
#define FREESPACE2_UTIL_FMT_HH

#include <cstdarg>
#include <cstddef>
#include <string>

#include <defs.hh>

char* fs2_snfmt  (char*, size_t, char const*, ...);
char* fs2_vsnfmt (char*, size_t, char const*, va_list);

std::string
fs2_fmt (const char*, int, const char* = 0, ...);

#endif // FREESPACE2_UTIL_FMT_HH
