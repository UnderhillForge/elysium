#pragma once

#include <glm/glm.hpp>

namespace elysium {

class Camera {
public:
    void setAspect(float aspect);

    void updateFly(float dt, bool active);
    void updateOrbitPanZoom(float dt, bool active);

    glm::mat4 viewMatrix() const;
    glm::mat4 projectionMatrix() const;
    glm::vec3 position() const { return m_position; }
    glm::vec3 forwardVector() const;
    glm::vec3 upVector() const;
    float nearPlane() const { return m_near; }
    float farPlane() const { return m_far; }
    // Yaw of the camera in degrees. Used by CharacterController so WASD is
    // always relative to the screen-forward direction.
    float yawDeg() const { return m_yawDeg; }

private:
    float m_aspect{16.0f / 9.0f};
    float m_fovDeg{60.0f};
    float m_near{0.05f};
    float m_far{2000.0f};

    glm::vec3 m_position{0.0f, 2.0f, 6.0f};
    float m_yawDeg{-90.0f};
    float m_pitchDeg{-15.0f};

    glm::vec3 m_orbitTarget{0.0f, 0.0f, 0.0f};
};

} // namespace elysium
