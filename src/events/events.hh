// -*- mode: c++; -*-

#ifndef FREESPACE2_EVENTS_EVENTS_HH
#define FREESPACE2_EVENTS_EVENTS_HH

#include <defs.hh>

#include <util/event.hh>

namespace events {

extern util::event< void > EngineUpdate;

extern util::event< void > EngineShutdown;

extern util::event< void, int, int > GameLeaveState;

extern util::event< void, int, int > GameEnterState;

extern util::event< void, const char* > GameMissionLoad;
} // namespace events

#endif // FREESPACE2_EVENTS_EVENTS_HH
