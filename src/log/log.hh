// -*- mode: c++; -*-

#ifndef FREESPACE2_LOG_LOG_HH
#define FREESPACE2_LOG_LOG_HH

#include <boost/log/trivial.hpp>
#include <boost/log/sources/global_logger_storage.hpp>
#include <boost/log/sources/severity_channel_logger.hpp>
#include <string>

#include <defs.hh>
#include <util/fmt.hh>

using fs2_logger_type = boost::log::sources::severity_channel_logger_mt<
    boost::log::trivial::severity_level, std::string >;

BOOST_LOG_GLOBAL_LOGGER(fs2_logger, fs2_logger_type)

#define FS2_LOG(channel, severity) BOOST_LOG_CHANNEL_SEV( \
        fs2_logger::get (), channel, boost::log::trivial::severity)

#define FS2_EE(channel) FS2_LOG (channel, error)
#define FS2_WW(channel) FS2_LOG (channel, warning)
#define FS2_II(channel) FS2_LOG (channel, info)

#define FS2_ERROR FS2_EE
#define FS2_WARN  FS2_WW
#define FS2_INFO  FS2_II

#define EE  FS2_EE("general")
#define WW  FS2_WW("general")
#define II  FS2_II("general")

#ifndef NDEBUG
#  define ERRORF   EE << fs2_fmt
#  define WARNINGF WW << fs2_fmt
#  define RELEASE_WARNINGF WARNINGF
#else
#  define ERRORF(...)   ((void)0)
#  define WARNINGF(...) ((void)0)
#  define RELEASE_WARNINGF(...) ((void)0)
#endif // NDEBUG

#endif // FREESPACE2_LOG_LOG_HH
