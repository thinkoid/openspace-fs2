// -*- mode: c++; -*-

#ifndef FREESPACE2_OSAPI_OSREGISTRY_HH
#define FREESPACE2_OSAPI_OSREGISTRY_HH

#include <defs.hh>

#include <string>

#include <boost/property_tree/ptree.hpp>
namespace pt = boost::property_tree;

namespace fs2 {

using registry_type = pt::ptree;

namespace registry {

std::string
read (const std::string&, const std::string& = { });

int
read (const std::string&, int = 0);

float
read (const std::string&, float = 0.f);

void
write (const std::string&, const std::string&);

void
write (const std::string&, int);

void
write (const std::string&, float);

}}

#endif // FREESPACE2_OSAPI_OSREGISTRY_HH
