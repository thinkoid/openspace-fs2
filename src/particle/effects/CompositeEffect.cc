// -*- mode: c++; -*-

#include <defs.hh>

#include <particle/particle.hh>
#include <particle/ParticleEffect.hh>
#include <particle/ParticleSource.hh>
#include "particle/effects/CompositeEffect.hh"

namespace particle {
namespace effects {
CompositeEffect::CompositeEffect (const std::string& name)
    : ParticleEffect (name) {}

bool CompositeEffect::processSource (const ParticleSource*) {
    ASSERT (0);
    return false;
}

void CompositeEffect::parseValues (bool) {
    while (optional_string ("+Child effect:")) {
        auto effectId = internal::parseEffectElement ();
        if (effectId.isValid ()) {
            ParticleEffectPtr effect =
                ParticleManager::get ()->getEffect (effectId);

            if (effect->getType () == EffectType::Composite) {
                error_display (
                    0,
                    "A composite effect cannot contain more composite "
                    "effects! The effect as not been added.");
            }
            else {
                addEffect (effect);
            }
        }
    }
}

void CompositeEffect::pageIn () {
    for (auto& effect : m_childEffects) { effect->pageIn (); }
}

void CompositeEffect::addEffect (ParticleEffectPtr effect) {
    m_childEffects.push_back (effect);
}
} // namespace effects
} // namespace particle
