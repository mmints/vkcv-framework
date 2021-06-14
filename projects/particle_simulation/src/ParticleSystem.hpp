#pragma once

#include <vector>
#include "Particle.hpp"

class ParticleSystem {

public:
    const std::vector<Particle> &getParticles() const;
    void addParticle( const Particle particle );
    void addParticles( const std::vector<Particle> particles );
    void updateParticles( const float deltaTime );
    void setRespawnPos( const glm::vec3 respawnPos );
    void setMaxLifeTime( const float respawnTime );

private:
    std::vector<Particle> m_particles;
    glm::vec3 m_respawnPos = glm::vec3(0.f);
    float m_maxLifeTime = 1.f;
};