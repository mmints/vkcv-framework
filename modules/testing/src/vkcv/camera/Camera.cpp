#include "vkcv/camera/Camera.hpp"

namespace vkcv {

    Camera::Camera() = default;

    Camera::~Camera() = default;

    void Camera::lookAt(glm::vec3 position, glm::vec3 center, glm::vec3 up){
        m_view = glm::lookAt(position, center, up);
    }

    void Camera::getView(glm::vec3 &x, glm::vec3 &y, glm::vec3 &z, glm::vec3 &pos){
        x = glm::vec3( glm::row(m_view, 0));
        y = glm::vec3( glm::row(m_view, 1));
        z = glm::vec3( glm::row(m_view, 2));
        pos = glm::vec3( glm::column(m_view, 3));
        glm::mat3 mat_inv = glm::inverse( glm::mat3(m_view));
        pos = -mat_inv * pos;
    }

    void Camera::getNearFar( float &near, float &far)
    {
        near = m_near;
        far = m_far;
    }


    const glm::mat4& Camera::getView() {
        return m_view;
    }

    void Camera::setView( const glm::mat4 view ) {
        m_view = view;
    }

    const glm::mat4& Camera::getProjection() {
        return m_projection;
    }

    void Camera::setProjection(const glm::mat4 projection) {
        m_projection = projection;
    }

    float Camera::getFov(){
        return m_fov;
    }

    void Camera::setFov( float fov){
        m_fov = fov;
        setPerspective( m_fov, m_ratio, m_near, m_far);
    }

    void Camera::updateRatio( float ratio){
        m_ratio = ratio;
        setPerspective( m_fov, m_ratio, m_near, m_far);
    }

    float Camera::getRatio(){
        return 0.0f;
    }

    void Camera::setNearFar( float near, float far){
        m_near = near;
        m_far = far;
        setPerspective(m_fov, m_ratio, m_near, m_far);
    }

}