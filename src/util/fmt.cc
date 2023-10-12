// -*- mode: c++; -*-

#include <filesystem>
#include <cstdarg>

#include <defs.hh>
#include <log/log.hh>
#include <util/fmt.hh>
#include "config.hh"
#include <log/log.hh>

namespace fs = std::filesystem;

#include <boost/noncopyable.hpp>

char*
fs2_vsnfmt (char* pbuf, size_t len, char const* fmt, va_list ap) {
    if (0 == fmt || 0 == fmt [0]) {
        return 0;
    }

    char buf [512], *psave = pbuf;

    if (0 == pbuf || 0 == len) {
        pbuf = buf;
        len = sizeof buf;
    }

    for (;;) {
        int n = 0;

        va_list tmp_va;
        FS2_VA_COPY (tmp_va, ap);

        n = FS2_VSNPRINTF (pbuf, len, fmt, tmp_va);
        va_end (tmp_va);

        if (0 > n) {
#if !defined (FS2_NO_POSIX_VSNPRINTF)
            //
            // Conversion error -- abort.
            //
            pbuf [0] = 0;
            break;
#else
            //
            // Non-conformant implementations may return a negative
            // value on buffer overflow (e.g., Windows).
            //
            n = 0;
#endif // FS2_NO_POSIX_VSNPRINTF
        }

        if (0 == n) {
            len *= 2;
        }
        else if (n >= int (len)) {
            len = n + 1;
        }
        else {
            break;
        }

        if (pbuf != buf && pbuf != psave) {
            free (pbuf);
            pbuf = 0;
        }

#if defined (FS2_NO_POSIX_VSNPRINTF)
        if (len > FS2_VSNFMT_MAX) {
            //
            // If vsnprintf overflow feed-back is missing or otherwise
            // unusable, limit the growth of the buffer and the number
            // of retries.
            //
            break;
        }
#endif // FS2_NO_POSIX_VSNPRINTF

        WW << "vsnfmt malloc : " << len;

        pbuf = reinterpret_cast< char* > (malloc (len));

        if (0 == pbuf)
            return 0;
    }

    if (pbuf == buf) {
        pbuf = reinterpret_cast< char* > (malloc (strlen (pbuf) + 1));

        if (0 == pbuf)
            return 0;

        strcpy (pbuf, buf);
    }

    return pbuf;
}

char*
fs2_snfmt (char* pbuf, size_t len, char const* fmt, ...) {
    va_list ap;

    va_start (ap, fmt);
    char* ptr = fs2_vsnfmt (pbuf, len, fmt, ap);
    va_end (ap);

    return ptr;
}

////////////////////////////////////////////////////////////////////////

struct guard_t : private boost::noncopyable {
    guard_t () = delete;
    guard_t (char* p, bool b) : p_ (p), own_ (b) { }

    guard_t (const guard_t&) = delete;
    guard_t (guard_t&&) = delete;

    guard_t& operator= (const guard_t&) = delete;
    guard_t& operator= (guard_t&&) = delete;

    ~guard_t () {
        if (own_ && p_) free (p_);
    }

private:
    char* p_;
    bool own_;
};

std::string
fs2_fmt (const char* file, int line, const char* fmt, ...) {
    char buf [256], *pbuf = buf;

    va_list ap;

    va_start (ap, fmt);
    pbuf = fs2_vsnfmt (pbuf, sizeof buf, fmt, ap);
    va_end (ap);

    guard_t guard (pbuf, pbuf != buf);

    std::stringstream ss;
    ss << fs::path (file).filename () << ":" << line << " : " << pbuf;

    return ss.str ();
}

