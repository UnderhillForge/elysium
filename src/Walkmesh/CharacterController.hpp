#pragma once

#include <glm/glm.hpp>

namespace elysium {

class Walkmesh;

// Kinematic character controller for editor preview mode.
//
// Movement model:
//  - WASD input moves the character in XZ space relative to the editor camera yaw,
//    so "forward" always matches what the user sees on screen.
//  - Every frame the character's Y is snapped to the walkmesh surface directly below.
//  - If no walkmesh triangle is found (off an edge), gravity accumulates until
//    the character falls back onto a surface.
//  - Horizontal movement is blocked if the step-up required exceeds stepHeight.
//
// Future extensions planned but deferred:
//  - Capsule radius sweep for wall sliding
//  - Jump / swim states
//  - Smooth stair auto-climb
class CharacterController {
public:
    struct Settings {
        float moveSpeed  {4.0f};  // m/s horizontal movement
        float eyeHeight  {1.7f};  // distance above foot position for 1st-person camera
        float stepHeight {0.4f};  // max upward step the character can take per frame
        float fallSpeed  {9.8f};  // gravitational acceleration (m/s²)
    };

    // Reset to a given world position and zero velocity.
    void reset(const glm::vec3& startPos);

    // Advance the simulation one timestep.
    // active: process keyboard input only when the viewport is focused.
    // cameraYawDeg: editor camera yaw so WASD is screen-relative.
    void update(float dt, const Walkmesh& walkmesh, float cameraYawDeg, bool active);

    glm::vec3 footPosition() const { return m_position; }
    glm::vec3 eyePosition()  const { return {m_position.x, m_position.y + m_settings.eyeHeight, m_position.z}; }
    float     facingYawDeg() const { return m_facingYawDeg; }

    Settings&       settings()       { return m_settings; }
    const Settings& settings() const { return m_settings; }

private:
    glm::vec3 m_position     {0.0f};
    float     m_facingYawDeg {0.0f};
    float     m_verticalVel  {0.0f};
    Settings  m_settings;
};

} // namespace elysium
