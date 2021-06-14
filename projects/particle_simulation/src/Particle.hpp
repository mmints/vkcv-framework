#pragma once

#include <glm/glm.hpp>

class Particle {

public:
    Particle(glm::vec3 position, glm::vec3 velocity, float lifeTime = 1.f);

    const glm::vec3& getPosition()const;

    void setPosition( const glm::vec3 pos );

    void update( const float delta );

    const bool isAlive()const;

    void setLifeTime( const float lifeTime );

    const float& getLifeTime()const;

private:
    // all properties of the Particle
    glm::vec3 m_position;
    float m_lifeTime;
    glm::vec3 m_velocity;
    float padding_2 = 0.f;
};