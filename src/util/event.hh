// -*- mode: c++; -*-

#ifndef FREESPACE2_UTIL_EVENT_HH
#define FREESPACE2_UTIL_EVENT_HH

#include <defs.hh>

#include <functional>

namespace util {

template< typename T, typename... Args >
struct event {
    using callback_type = std::function< T (Args...) >;

    void listen (callback_type func) { fs.push_back (func); }

    void clear () { fs.clear (); }

    template< typename U = void >
    inline typename std::enable_if< std::is_same< T, void >::value, U >::type
    notify_all (Args... args) const {
        for (const auto& f : fs) f (args...);
    }

    template< typename U = std::vector< T > >
    inline typename std::enable_if< !std::is_same< T, void >::value, U >::type
    notify_all (Args... args) const {
        std::vector< T > result;

        for (const auto& f : fs) result.push_back (f (args...));

        return result;
    }

private:
    std::vector< callback_type > fs;
};

} // namespace util

#endif // FREESPACE2_UTIL_EVENT_HH
