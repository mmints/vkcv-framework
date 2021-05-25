#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_access.hpp>

namespace vkcv {

    class Camera {
    protected:
		glm::mat4 m_view;
		glm::mat4 m_projection;

		int m_width;
		int m_height;

		float m_oldX;
		float m_oldY;
		float m_near;
		float m_far;
		float m_fov;
		float m_ratio;

		glm::vec3 m_position;
		glm::vec3 m_direction;
		glm::vec3 m_up;

    public:
        Camera();

        ~Camera();

        void setPerspective(float fov, float ratio, float near, float far);

        const glm::mat4 &getView();

        void getView(glm::vec3 &x, glm::vec3 &y, glm::vec3 &z, glm::vec3 &pos);

        void setView(const glm::mat4 view);

        void lookAt(glm::vec3 position, glm::vec3 center, glm::vec3 up);

        const glm::mat4& getProjection();

        void setProjection(const glm::mat4 projection);

        void getNearFar(float &near, float &far);

        float getFov();

        void setFov(float fov);

        void updateRatio(float ratio);

        float getRatio();

        void setNearFar(float near, float far);

    };

}
