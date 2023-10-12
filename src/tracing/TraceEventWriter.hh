// -*- mode: c++; -*-

#ifndef FREESPACE2_TRACING_TRACEEVENTWRITER_HH
#define FREESPACE2_TRACING_TRACEEVENTWRITER_HH

#include <defs.hh>

#include <tracing/tracing.hh>
#include <tracing/ThreadedEventProcessor.hh>

#include <fstream>

/** @file
 *  @ingroup tracing
 */

namespace tracing {
class TraceEventWriter {
    std::ofstream _out;
    bool _first_line = true;

public:
    TraceEventWriter ();
    ~TraceEventWriter ();

    void processEvent (const trace_event* event);
};

typedef ThreadedEventProcessor< TraceEventWriter > ThreadedTraceEventWriter;
} // namespace tracing

#endif // FREESPACE2_TRACING_TRACEEVENTWRITER_HH
