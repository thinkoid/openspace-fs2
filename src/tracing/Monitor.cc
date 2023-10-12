// -*- mode: c++; -*-

#include <tracing/Monitor.hh>
#include <tracing/tracing.hh>

using namespace tracing;

namespace tracing {

MonitorBase::MonitorBase (const char* name)
    : _name (name), _tracing_cat (name, false) {}
void MonitorBase::valueChanged (float newVal) {
    tracing::counter::value (_tracing_cat, newVal);
}

} // namespace tracing
