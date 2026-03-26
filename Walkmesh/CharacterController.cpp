#include "Walkmesh/CharacterController.hpp"
#include "Walkmesh/Walkmesh.hpp"

#include <imgui.h>

#include <cmath>

namespace elysium {

void CharacterController::reset(const glm::vec3& startPos) {
    m_position     = startPos;
    m_facingYawDeg = 0.0f;
    m_verticalVel  = 0.0f;
}

void CharacterController::update(float dt, const Walkmesh& walkmesh,
                                  float cameraYawDeg, bool active) {
    // Build camera-relative movement directions so WASD matches screen orientation.
    const float rad = cameraYawDeg * 3.14159265f / 180.0f;
    const glm::vec3 fwd{-std::sin(rad), 0.0f, -std::cos(rad)};
    const glm::vec3 rgt{ std::cos(rad), 0.0f, -std::sin(rad)};

    // Accumulate WASD input from ImGui (safe to call between NewFrame/Render).
    glm::vec3 wishDir{0.0f};
    if (active) {
        // W/E/R are used by the gizmo shortcuts; P by preview toggle.
        // WASD is claimed exclusively by the character controller in preview mode.
        if (ImGui::IsKeyDown(ImGuiKey_W)) wishDir += fwd;
        if (ImGui::IsKeyDown(ImGuiKey_S)) wishDir -= fwd;
        if (ImGui::IsKeyDown(ImGuiKey_A)) wishDir -= rgt;
        if (ImGui::IsKeyDown(ImGuiKey_D)) wishDir += rgt;
    }

    const float len = wishDir.x * wishDir.x + wishDir.z * wishDir.z;
    if (len > 0.0001f) {
        const float invLen = 1.0f / std::sqrt(len);
        wishDir.x *= invLen;
        wishDir.z *= invLen;
        // Update the character's facing direction while moving.
        m_facingYawDeg = std::atan2(-wishDir.x, -wishDir.z) * 180.0f / 3.14159265f;
    }

    // Proposed next XZ position.
    const float nextX = m_position.x + wishDir.x * m_settings.moveSpeed * dt;
    const float nextZ = m_position.z + wishDir.z * m_settings.moveSpeed * dt;

    const auto groundOpt = walkmesh.sampleHeight(nextX, nextZ);
    if (groundOpt.has_value()) {
        const float groundY  = groundOpt.value();
        const float stepDiff = groundY - m_position.y;
        if (stepDiff <= m_settings.stepHeight) {
            // Accept move: snap character to surface.
            m_position.x  = nextX;
            m_position.z  = nextZ;
            m_position.y  = groundY;
            m_verticalVel = 0.0f;
        }
        // else: step too steep — horizontal movement blocked, no crash.
    } else {
        // Nothing under candidate position — allow horizontal slide but apply gravity.
        m_position.x   = nextX;
        m_position.z   = nextZ;
        m_verticalVel -= m_settings.fallSpeed * dt;
        m_position.y  += m_verticalVel * dt;

        // Re-check surface after Y update in case we fell through.
        const auto belowOpt = walkmesh.sampleHeight(m_position.x, m_position.z);
        if (belowOpt.has_value() && m_position.y <= belowOpt.value()) {
            m_position.y  = belowOpt.value();
            m_verticalVel = 0.0f;
        }
    }
}

} // namespace elysium
