// -*- mode: c++; -*-

#ifndef FREESPACE2_ASSERT_ASSERT_HH
#define FREESPACE2_ASSERT_ASSERT_HH

#include <defs.hh>

#ifndef NDEBUG

void
assert_format (const char*, const char*, int, const char* = 0, ...)
    __attribute__ ((format (printf, 4, 5)));

#  define ASSERTX(expr, fmt, ...)                               \
    (static_cast< bool > ((expr)) ? void (0) : assert_format (  \
         #expr, __FILE__, __LINE__, fmt, ##__VA_ARGS__))
#else
#  define ASSERTX(expr, ignore, ...) FS2_UNUSED ((expr))
#endif // NDEBUG

#endif // FREESPACE2_ASSERT_ASSERT_HH
