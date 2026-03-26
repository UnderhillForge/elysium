#pragma once

#include <glm/glm.hpp>

namespace elysium {

struct Transform {
    glm::vec3 position{0.0f};
    glm::vec3 rotationEulerDeg{0.0f};
    glm::vec3 scale{1.0f};

    glm::mat4 localMatrix() const;
};

} // namespace elysium
