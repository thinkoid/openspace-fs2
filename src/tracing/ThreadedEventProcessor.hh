// -*- mode: c++; -*-

#ifndef FREESPACE2_TRACING_THREADEDEVENTPROCESSOR_HH
#define FREESPACE2_TRACING_THREADEDEVENTPROCESSOR_HH

#include <defs.hh>

#include <tracing/tracing.hh>

#include <thread>

#include <boost/thread/sync_bounded_queue.hpp>
#include <boost/thread/concurrent_queues/queue_op_status.hpp>

/** @file
 *  @ingroup tracing
 */

namespace tracing {

/**
 * @brief A multi-threaded event processor
 *
 * This is a utility class that can be used to implement an event processor
 * that uses a different thread to process the events. To use this, declare
 * your class with a method with the signature
 *
 * @code{.cpp}
 * void processEvent(const trace_event* event)
 * @endcode
 *
 * This function will be called in a background-thread whenever a new event
 * arrives.
 *
 * @tparam Processor Your processor implementation
 * @tparam N The maximum size of the internal event buffer
 */
template< class Processor, size_t N = 200 >
struct ThreadedEventProcessor {
    template< typename... Params >
    explicit ThreadedEventProcessor (Params&&... params)
        : q_ (N),
          w_ (&ThreadedEventProcessor< Processor >::workerThread, this),
          p_ (std::forward< Params > (params)...) {}

    ~ThreadedEventProcessor () {
        q_.close ();
        w_.join ();
    }

    void processEvent (const trace_event* event) {
        using namespace boost::concurrent;

        try {
            q_.wait_push_back (*event);
        }
        catch (const sync_queue_is_closed&) {
            WARNINGF (LOCATION,"Stream queue was closed in processEvent! This should not be possible...");
        }
    }

private:
    void workerThread () {
        using namespace boost;
        using boost::concurrent::sync_queue_is_closed;

        while (!q_.closed ()) {
            try {
                trace_event evt;

                if (queue_op_status::success != q_.wait_pull_front (evt))
                    break;

                p_.processEvent (&evt);
            }
            catch (const sync_queue_is_closed&) {
                break;
            }
        }
    }

    boost::sync_bounded_queue< trace_event > q_;
    std::thread w_;
    Processor p_;
};

} // namespace tracing

#endif // FREESPACE2_TRACING_THREADEDEVENTPROCESSOR_HH
