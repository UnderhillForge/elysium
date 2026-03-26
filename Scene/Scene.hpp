#pragma once

#include "Assets/GLTFLoader.hpp"
#include "Scene/Transform.hpp"

#include <array>
#include <cstdint>
#include <glm/glm.hpp>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace elysium {

using SceneEntityId = std::uint64_t;

struct SceneEntity {
    SceneEntityId id{0};
    std::string name{"Entity"};
    std::array<char, 128> nameBuffer{};
    std::string sourceAssetPath;
    std::shared_ptr<GLTFModel> model;

    struct ScriptComponent {
        bool enabled{false};
        std::string scriptPath;
    };
    std::optional<ScriptComponent> script;

    struct PhysicsComponent {
        bool enabled{false};
        bool dynamic{true};
        glm::vec3 halfExtents{0.5f, 0.5f, 0.5f};
    };
    std::optional<PhysicsComponent> physics;


    struct LightComponent {
        enum class Type : int { Point = 0, Directional = 1 };
        Type      type{Type::Point};
        glm::vec3 color{1.0f, 1.0f, 1.0f};
        float     intensity{1.0f};
        float     range{10.0f};  // point-light only
        bool      enabled{true};
    };
    std::optional<LightComponent> light;

    struct ParticleEmitterComponent {
        enum class Preset : int { Custom = 0, Fire = 1, Smoke = 2, Sparks = 3, Magic = 4 };
        enum class BlendMode : int { Alpha = 0, Additive = 1, Premultiplied = 2 };

        bool enabled{true};
        Preset preset{Preset::Custom};
        BlendMode blendMode{BlendMode::Alpha};
        uint32_t maxParticles{10000};
        float emissionRate{100.0f};
        float particleLifetime{2.0f};
        glm::vec3 velocity{0.0f, 1.0f, 0.0f};
        glm::vec3 velocityVariance{0.5f, 0.2f, 0.5f};
        glm::vec3 positionVariance{0.1f, 0.1f, 0.1f};
        glm::vec3 acceleration{0.0f, -0.98f, 0.0f};
        float damping{0.99f};
        bool looping{true};
        uint32_t burstCount{0};
        float burstInterval{1.0f};
        bool burstOnStart{false};
        glm::vec4 colorStart{1.0f, 1.0f, 1.0f, 1.0f};
        glm::vec4 colorMid{1.0f, 1.0f, 1.0f, 0.65f};
        glm::vec4 colorEnd{1.0f, 1.0f, 1.0f, 0.0f};
        float colorMidPoint{0.45f};
        float sizeStart{0.1f};
        float sizeMid{0.08f};
        float sizeEnd{0.05f};
        float sizeMidPoint{0.45f};
        int spriteColumns{1};
        int spriteRows{1};
        float spriteFrameRate{0.0f};
        bool randomStartFrame{false};
        std::string spriteTexturePath{};
        bool collideWithGround{false};
        glm::vec3 collisionPlaneNormal{0.0f, 1.0f, 0.0f};
        float collisionPlaneOffset{0.0f};
        float bounceFactor{0.35f};
        float collisionFade{0.45f};
        int maxBounces{1};
    };
    std::optional<ParticleEmitterComponent> particleEmitter;

    struct AudioComponent {
        bool enabled{true};
        std::string clipPath;
        float gain{1.0f};
        float minDistance{1.0f};
        float maxDistance{40.0f};
        bool looping{false};
    };
    std::optional<AudioComponent> audio;

    Transform transform{};
    glm::vec4 tint{1.0f};

    void syncNameBuffer();
};

class Scene {
public:
    SceneEntityId createEntity(const std::string& name, const std::shared_ptr<GLTFModel>& model);
    bool destroyEntity(SceneEntityId id);
    void clear();

    std::vector<SceneEntity>& entities() { return m_entities; }
    const std::vector<SceneEntity>& entities() const { return m_entities; }

    SceneEntity* findEntity(SceneEntityId id);
    const SceneEntity* findEntity(SceneEntityId id) const;

    void draw(GLuint shaderProgram) const;

private:
    std::vector<SceneEntity> m_entities;
    SceneEntityId m_nextEntityId{1};
};

} // namespace elysium
