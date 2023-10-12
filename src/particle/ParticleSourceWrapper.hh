// -*- mode: c++; -*-

#ifndef FREESPACE2_PARTICLE_PARTICLESOURCEWRAPPER_HH
#define FREESPACE2_PARTICLE_PARTICLESOURCEWRAPPER_HH

#include <defs.hh>

#include "ParticleSource.hh"

enum class WeaponState : uint32_t;

namespace particle {
class ParticleSource;

/**
 * @brief A wrapper around multiple particle sources
 *
 * This class contains multiple sources which are grouped together. This is
 * needed because effects may create multiple sources and this class provides
 * transparent handling of that case.
 *
 * Once initialization of the sources is done you must call #finish() in order
 * to mark the sources as properly initialized.
 *
 * @ingroup particleSystems
 */
class ParticleSourceWrapper {
private:
    std::vector< ParticleSource* > m_sources;

    bool m_finished = false;

public:
    ParticleSourceWrapper (const ParticleSourceWrapper&) = delete;

    ParticleSourceWrapper& operator= (const ParticleSourceWrapper&) = delete;

    ParticleSourceWrapper () {}
    explicit ParticleSourceWrapper (std::vector< ParticleSource* >&& sources);
    explicit ParticleSourceWrapper (ParticleSource* source);

    ~ParticleSourceWrapper ();

    ParticleSourceWrapper (ParticleSourceWrapper&& other) noexcept;

    ParticleSourceWrapper& operator= (ParticleSourceWrapper&& other) noexcept;

    void finish ();

    void setCreationTimestamp (int timestamp);

    void moveToParticle (const WeakParticlePtr& ptr);

    void moveToObject (object* obj, vec3d* localPos);

    void moveTo (vec3d* pos);

    void setOrientationFromNormalizedVec (vec3d* normalizedDir);

    void setOrientationFromVec (vec3d* dir);

    void setOrientationMatrix (matrix* mtx);

    void setOrientationNormal (vec3d* normal);

    void setWeaponState (WeaponState state);
};
} // namespace particle

#endif // FREESPACE2_PARTICLE_PARTICLESOURCEWRAPPER_HH
