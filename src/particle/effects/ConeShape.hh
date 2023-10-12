// -*- mode: c++; -*-

#ifndef FREESPACE2_PARTICLE_EFFECTS_CONESHAPE_HH
#define FREESPACE2_PARTICLE_EFFECTS_CONESHAPE_HH

#include <defs.hh>

#include <util/RandomRange.hh>

namespace particle {
namespace effects {

/**
 * @ingroup particleEffects
 */
class ConeShape {
    ::util::NormalFloatRange m_normalDeviation;

public:
    ConeShape () {}

    matrix getDisplacementMatrix () {
        angles_t angs;

        angs.b = 0.0f;

        angs.h = m_normalDeviation.next ();
        angs.p = m_normalDeviation.next ();

        matrix m;

        vm_angles_2_matrix (&m, &angs);

        return m;
    }

    void parse (bool nocreate) {
        if (internal::required_string_if_new ("+Deviation:", nocreate)) {
            float deviation;
            stuff_float (&deviation);

            m_normalDeviation =
                ::util::NormalFloatRange (0.0, to_radians (deviation));
        }
    }

    EffectType getType () const { return EffectType::Cone; }

    /**
     * @brief Specifies if the velocities of the particles should be scaled
     * with the deviation from the direction
     * @return @c true
     */
    static constexpr bool scale_velocity_deviation () { return true; }
};

} // namespace effects
} // namespace particle

#endif // FREESPACE2_PARTICLE_EFFECTS_CONESHAPE_HH
