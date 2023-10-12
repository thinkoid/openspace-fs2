// -*- mode: c++; -*-

#ifndef FREESPACE2_PARTICLE_EFFECTS_SINGLEPARTICLEEFFECT_HH
#define FREESPACE2_PARTICLE_EFFECTS_SINGLEPARTICLEEFFECT_HH

#include <defs.hh>

#include <particle/ParticleEffect.hh>
#include <particle/ParticleManager.hh>
#include "particle/util/ParticleProperties.hh"
#include "particle/util/EffectTiming.hh"

namespace particle {
namespace effects {
/**
 * @ingroup particleEffects
 */
class SingleParticleEffect : public ParticleEffect {
private:
    util::ParticleProperties m_particleProperties;

    util::EffectTiming m_timing;

public:
    explicit SingleParticleEffect (const std::string& name);

    bool processSource (const ParticleSource* source) override;

    void parseValues (bool nocreate) override;

    void pageIn () override;

    void initializeSource (ParticleSource& source) override;

    EffectType getType () const override { return EffectType::Single; }

    util::ParticleProperties& getProperties () { return m_particleProperties; }

    static SingleParticleEffect* createInstance (
        int effectID, float minSize, float maxSize, float lifetime = -1.0f);
};
} // namespace effects
} // namespace particle

#endif // FREESPACE2_PARTICLE_EFFECTS_SINGLEPARTICLEEFFECT_HH
