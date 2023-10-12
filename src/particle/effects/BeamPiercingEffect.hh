// -*- mode: c++; -*-

#ifndef FREESPACE2_PARTICLE_EFFECTS_BEAMPIERCINGEFFECT_HH
#define FREESPACE2_PARTICLE_EFFECTS_BEAMPIERCINGEFFECT_HH

#include <defs.hh>

#include <particle/ParticleEffect.hh>

namespace particle {
namespace effects {
/**
 * @ingroup particleEffects
 */
class BeamPiercingEffect : public ParticleEffect {
private:
    float m_radius = -1.f;
    float m_velocity = -1.f;
    float m_backVelocity = -1.f;
    float m_variance = -1.f;

    int m_effectBitmap = -1;

public:
    BeamPiercingEffect () : ParticleEffect ("") {}

    bool processSource (const ParticleSource* source) override;

    void parseValues (bool nocreate) override;

    void pageIn () override;

    void setValues (
        int bitmapIndex, float radius, float velocity, float back_velocity,
        float variance);
};
} // namespace effects
} // namespace particle

#endif // FREESPACE2_PARTICLE_EFFECTS_BEAMPIERCINGEFFECT_HH
