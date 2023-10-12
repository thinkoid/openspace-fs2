// -*- mode: c++; -*-

#include <defs.hh>

#include <particle/ParticleSource.hh>
#include "particle/effects/ParticleEmitterEffect.hh"
#include <bmpman/bmpman.hh>
#include <parse/parselo.hh>

namespace particle {
namespace effects {
ParticleEmitterEffect::ParticleEmitterEffect () : ParticleEffect ("") {
    memset (&m_emitter, 0, sizeof (m_emitter));
}

bool ParticleEmitterEffect::processSource (const ParticleSource* source) {
    particle_emitter emitter = m_emitter;
    source->getOrigin ()->getGlobalPosition (&emitter.pos);
    emitter.normal =
        source->getOrientation ()->getDirectionVector (source->getOrigin ());

    emit (&emitter, PARTICLE_BITMAP, m_particleBitmap, m_range);

    return false;
}

void ParticleEmitterEffect::parseValues (bool) {
    error_display (
        1, "Parsing not implemented for this effect because I'm lazy...");
}

void ParticleEmitterEffect::pageIn () {
    bm_page_in_texture (m_particleBitmap);
}

void ParticleEmitterEffect::setValues (
    const particle_emitter& emitter, int bitmap, float range) {
    ASSERT (bm_is_valid (bitmap));

    m_emitter = emitter;
    m_particleBitmap = bitmap;
    m_range = range;
}
} // namespace effects
} // namespace particle
