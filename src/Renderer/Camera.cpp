#include "Renderer/Camera.hpp"

#include <imgui.h>
#include <glm/gtc/matrix_transform.hpp>

#include <cmath>

namespace elysium {

void Camera::setAspect(float aspect) {
    m_aspect = (aspect > 0.0f) ? aspect : (16.0f / 9.0f);
}

void Camera::updateFly(float dt, bool active) {
    if (!active) {
        return;
    }

    ImGuiIO& io = ImGui::GetIO();

    if (!ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
        return;
    }

    const float lookSpeed = 0.12f;
    m_yawDeg += io.MouseDelta.x * lookSpeed;
    m_pitchDeg -= io.MouseDelta.y * lookSpeed;
    m_pitchDeg = glm::clamp(m_pitchDeg, -89.0f, 89.0f);

    glm::vec3 forward{
        std::cos(glm::radians(m_yawDeg)) * std::cos(glm::radians(m_pitchDeg)),
        std::sin(glm::radians(m_pitchDeg)),
        std::sin(glm::radians(m_yawDeg)) * std::cos(glm::radians(m_pitchDeg))
    };
    forward = glm::normalize(forward);
    glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0, 1, 0)));
    glm::vec3 up = glm::normalize(glm::cross(right, forward));

    float speed = io.KeyShift ? 12.0f : 6.0f;
    glm::vec3 delta{0.0f};

    if (ImGui::IsKeyDown(ImGuiKey_W)) delta += forward;
    if (ImGui::IsKeyDown(ImGuiKey_S)) delta -= forward;
    if (ImGui::IsKeyDown(ImGuiKey_D)) delta += right;
    if (ImGui::IsKeyDown(ImGuiKey_A)) delta -= right;
    if (ImGui::IsKeyDown(ImGuiKey_E)) delta += up;
    if (ImGui::IsKeyDown(ImGuiKey_Q)) delta -= up;

    if (glm::length(delta) > 0.0f) {
        m_position += glm::normalize(delta) * speed * dt;
    }
}

void Camera::updateOrbitPanZoom(float dt, bool active) {
    (void)dt;
    if (!active) {
        return;
    }

    ImGuiIO& io = ImGui::GetIO();

    // Alt + LMB orbit around pivot target.
    if (io.KeyAlt && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        m_yawDeg += io.MouseDelta.x * 0.15f;
        m_pitchDeg -= io.MouseDelta.y * 0.15f;
        m_pitchDeg = glm::clamp(m_pitchDeg, -89.0f, 89.0f);

        float dist = glm::length(m_position - m_orbitTarget);
        glm::vec3 dir{
            std::cos(glm::radians(m_yawDeg)) * std::cos(glm::radians(m_pitchDeg)),
            std::sin(glm::radians(m_pitchDeg)),
            std::sin(glm::radians(m_yawDeg)) * std::cos(glm::radians(m_pitchDeg))
        };
        m_position = m_orbitTarget - glm::normalize(dir) * dist;
    }

    // MMB pan.
    if (ImGui::IsMouseDown(ImGuiMouseButton_Middle)) {
        glm::vec3 forward{
            std::cos(glm::radians(m_yawDeg)) * std::cos(glm::radians(m_pitchDeg)),
            std::sin(glm::radians(m_pitchDeg)),
            std::sin(glm::radians(m_yawDeg)) * std::cos(glm::radians(m_pitchDeg))
        };
        forward = glm::normalize(forward);
        glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0, 1, 0)));
        glm::vec3 up = glm::normalize(glm::cross(right, forward));

        const float panSpeed = 0.01f;
        glm::vec3 pan = (-right * io.MouseDelta.x + up * io.MouseDelta.y) * panSpeed;
        m_position += pan;
        m_orbitTarget += pan;
    }

    // Mouse wheel dolly.
    if (io.MouseWheel != 0.0f) {
        glm::vec3 viewDir = glm::normalize(m_orbitTarget - m_position);
        m_position += viewDir * io.MouseWheel * 0.6f;
    }
}

glm::mat4 Camera::viewMatrix() const {
    glm::vec3 forward{
        std::cos(glm::radians(m_yawDeg)) * std::cos(glm::radians(m_pitchDeg)),
        std::sin(glm::radians(m_pitchDeg)),
        std::sin(glm::radians(m_yawDeg)) * std::cos(glm::radians(m_pitchDeg))
    };
    return glm::lookAt(m_position, m_position + glm::normalize(forward), glm::vec3(0, 1, 0));
}

glm::mat4 Camera::projectionMatrix() const {
    return glm::perspective(glm::radians(m_fovDeg), m_aspect, m_near, m_far);
}

glm::vec3 Camera::forwardVector() const {
    glm::vec3 forward{
        std::cos(glm::radians(m_yawDeg)) * std::cos(glm::radians(m_pitchDeg)),
        std::sin(glm::radians(m_pitchDeg)),
        std::sin(glm::radians(m_yawDeg)) * std::cos(glm::radians(m_pitchDeg))
    };
    return glm::normalize(forward);
}

glm::vec3 Camera::upVector() const {
    const glm::vec3 forward = forwardVector();
    const glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f)));
    return glm::normalize(glm::cross(right, forward));
}

} // namespace elysium
