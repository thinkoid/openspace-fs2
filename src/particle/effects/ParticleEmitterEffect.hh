// -*- mode: c++; -*-

#ifndef FREESPACE2_PARTICLE_EFFECTS_PARTICLEEMITTEREFFECT_HH
#define FREESPACE2_PARTICLE_EFFECTS_PARTICLEEMITTEREFFECT_HH

#include <defs.hh>

#include <particle/ParticleEffect.hh>
#include <particle/particle.hh>

namespace particle {
namespace effects {
/**
 * @ingroup particleEffects
 */
class ParticleEmitterEffect : public ParticleEffect {
private:
    particle_emitter m_emitter;
    int m_particleBitmap = -1;
    float m_range = -1;

public:
    ParticleEmitterEffect ();

    bool processSource (const ParticleSource* source) override;

    void parseValues (bool nocreate) override;

    void pageIn () override;

    void setValues (const particle_emitter& emitter, int bitmap, float range);
};
} // namespace effects
} // namespace particle

#endif // FREESPACE2_PARTICLE_EFFECTS_PARTICLEEMITTEREFFECT_HH
