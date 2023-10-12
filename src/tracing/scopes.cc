// -*- mode: c++; -*-

#include <tracing/scopes.hh>

namespace tracing {

Scope::Scope (const char* name) : _name (name) {}
Scope::~Scope () {}

Scope MainFrameScope ("main_frame");

} // namespace tracing
