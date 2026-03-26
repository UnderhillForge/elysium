#pragma once

#include <glm/glm.hpp>
#include <glad/glad.h>

#include <cstdint>
#include <vector>
#include <memory>
#include <optional>
#include <string>

namespace elysium {

// Particle data on GPU (must match compute shader layout)
struct GPUParticle {
    glm::vec4 positionLife;    // xyz = position, w = lifetime remaining
    glm::vec4 velocityAge;     // xyz = velocity, w = age (0..1)
    glm::vec4 colorSize;       // rgb = color, a = size
    glm::vec4 extraData;       // x = frame index, y = bounce count, z = collision fade, w = reserved
};

struct ParticleEmitterConfig {
    enum class BlendMode : int { Alpha = 0, Additive = 1, Premultiplied = 2 };

    glm::vec3 position{0.0f};
    glm::vec3 positionVariance{0.1f};
    glm::vec3 velocity{0.0f, 1.0f, 0.0f};
    glm::vec3 velocityVariance{0.5f, 0.2f, 0.5f};
    glm::vec3 acceleration{0.0f, -0.98f, 0.0f};    // gravity
    
    float particleLifetime{2.0f};
    float emissionRate{100.0f};   // particles per second
    bool looping{true};
    uint32_t burstCount{0};
    float burstInterval{1.0f};
    bool burstOnStart{false};
    BlendMode blendMode{BlendMode::Alpha};
    
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
    
    bool enabled{true};
    float damping{0.99f};    // velocity damping per frame
};

class ParticleEmitter {
public:
    ParticleEmitter(const ParticleEmitterConfig& config, uint32_t maxParticles = 10000);
    ~ParticleEmitter();

    void update(float dt);
    void render(const glm::mat4& viewProj,
                GLuint depthTexture,
                const glm::vec2& viewportSize,
                float nearPlane,
                float farPlane) const;

    void setConfig(const ParticleEmitterConfig& config);
    const ParticleEmitterConfig& config() const { return m_config; }
    
    uint32_t activeParticleCount() const { return m_activeCount; }
    uint32_t maxParticleCount() const { return m_maxParticles; }

private:
    ParticleEmitterConfig m_config;
    uint32_t m_maxParticles;
    uint32_t m_activeCount{0};
    float m_emissionAccumulator{0.0f};
    float m_burstAccumulator{0.0f};
    bool m_startBurstPending{true};

    // GPU resources
    GLuint m_particleSSBO{0};           // Shader Storage Buffer Object for particles
    GLuint m_indirectDrawBuffer{0};     // Indirect draw buffer for count
    GLuint m_computeShader{0};
    GLuint m_renderShader{0};
    GLuint m_billboardVAO{0};           // For billboard rendering
    GLuint m_spriteTexture{0};

    // Config uploaded to GPU
    GLuint m_configUBO{0};              // Uniform Buffer Object

    bool buildComputeShader();
    bool buildRenderShader();
    void buildBillboardGeometry();
    void buildSpriteTexture();
    bool loadSpriteTextureFromPath(const std::string& texturePath);
    void uploadConfigToGPU();
    void resetParticles();

    std::string m_loadedSpriteTexturePath;
};

class ParticleSystem {
public:
    static ParticleSystem& instance();

    void update(float dt);
    void render(const glm::mat4& viewProj,
                GLuint depthTexture,
                const glm::vec2& viewportSize,
                float nearPlane,
                float farPlane);

    std::shared_ptr<ParticleEmitter> createEmitter(const ParticleEmitterConfig& config, uint32_t maxParticles);
    void destroyEmitter(std::shared_ptr<ParticleEmitter> emitter);

    const std::vector<std::shared_ptr<ParticleEmitter>>& emitters() const { return m_emitters; }

private:
    ParticleSystem() = default;
    
    std::vector<std::shared_ptr<ParticleEmitter>> m_emitters;
};

} // namespace elysium
