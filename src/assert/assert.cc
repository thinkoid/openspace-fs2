// -*- mode: c++; -*-

#include <cstdarg>

#include <assert/assert.hh>
#include <log/log.hh>
#ifndef NDEBUG

#include <cstdarg>
#include <cstddef>

#include <log/log.hh>
#include <util/fmt.hh>

void
assert_format (
    const char* prefix, const char* file, int line, const char* fmt, ...) {
    va_list ap;

    va_start (ap, fmt);
    auto suffix = fs2_fmt (file, line, fmt, ap);
    va_end (ap);

    EE << prefix << " : " << suffix;
    
    std::terminate ();
}

#endif // NDEBUG
