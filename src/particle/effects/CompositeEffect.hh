// -*- mode: c++; -*-

#ifndef FREESPACE2_PARTICLE_EFFECTS_COMPOSITEEFFECT_HH
#define FREESPACE2_PARTICLE_EFFECTS_COMPOSITEEFFECT_HH

#include <defs.hh>

#include <particle/ParticleEffect.hh>
#include <particle/ParticleManager.hh>
#include <util/RandomRange.hh>

namespace particle {
namespace effects {
/**
 * @ingroup particleEffects
 */
class CompositeEffect : public ParticleEffect {
private:
    std::vector< ParticleEffectPtr > m_childEffects;

public:
    explicit CompositeEffect (const std::string& name);

    bool processSource (const ParticleSource* source) override;

    void parseValues (bool nocreate) override;

    void pageIn () override;

    EffectType getType () const override { return EffectType::Composite; }

    const std::vector< ParticleEffectPtr >& getEffects () const {
        return m_childEffects;
    }

    void addEffect (ParticleEffectPtr effect);
};
} // namespace effects
} // namespace particle

#endif // FREESPACE2_PARTICLE_EFFECTS_COMPOSITEEFFECT_HH
