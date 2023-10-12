// -*- mode: c++; -*-

#ifndef FREESPACE2_PARTICLE_UTIL_EFFECTTIMING_HH
#define FREESPACE2_PARTICLE_UTIL_EFFECTTIMING_HH

#include <defs.hh>

#include <particle/ParticleSource.hh>
#include <util/RandomRange.hh>

namespace particle {
namespace util {

/**
 * @defgroup particleUtils Particle Effect utilities
 *
 * @ingroup particleSystems
 */

/**
 * @brief The possible duration modes
 *
 * @ingroup particleUtils
 */
enum class Duration {
    Onetime, //!< The effect is active exactly once
    Range,   //!< The effect is active withing a specific time range
    Always   //!< The effect is always active
};

/**
 * @brief Class for managing the timing of an effect
 *
 * This allows to use a random time range for the duration of an effect
 *
 * @ingroup particleUtils
 */
class EffectTiming {
private:
    Duration m_duration;
    ::util::UniformFloatRange m_delayRange;
    ::util::UniformFloatRange m_durationRange;

public:
    EffectTiming ();

    /**
     * @brief Applies the timing information to a specific source
     * @param source The source to be modified
     */
    void applyToSource (ParticleSource* source);

    /**
     * @brief Determines if processing should continue
     * @param source The source which should be checked
     * @return @c true if processing should contine, @c false otherwise
     */
    bool continueProcessing (const ParticleSource* source);

    /**
     * @brief Parses an effect timing class
     * @return The parsed effect timing
     */
    static EffectTiming parseTiming ();
};
} // namespace util
} // namespace particle

#endif // FREESPACE2_PARTICLE_UTIL_EFFECTTIMING_HH
