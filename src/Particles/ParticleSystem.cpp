#include "ParticleSystem.hpp"

#include <algorithm>
#include <array>
#include <filesystem>
#include <glm/gtc/random.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <stb_image.h>
#include <cstring>
#include <iostream>

namespace elysium {

// ============================================================================
// PARTICLE EMITTER
// ============================================================================

ParticleEmitter::ParticleEmitter(const ParticleEmitterConfig& config, uint32_t maxParticles)
    : m_config(config), m_maxParticles(maxParticles), m_activeCount(0)
{
    // Create SSBO for particles
    glCreateBuffers(1, &m_particleSSBO);
    glNamedBufferData(m_particleSSBO, sizeof(GPUParticle) * maxParticles, nullptr, GL_DYNAMIC_DRAW);

    // Create indirect draw buffer (for compute shader to write particle count)
    glCreateBuffers(1, &m_indirectDrawBuffer);
    glNamedBufferData(m_indirectDrawBuffer, sizeof(uint32_t) * 4, nullptr, GL_DYNAMIC_DRAW);
    // Initialize: count, primCount, first, baseInstance
    uint32_t indirectData[4] = {0, 1, 0, 0};
    glNamedBufferSubData(m_indirectDrawBuffer, 0, sizeof(indirectData), indirectData);

    // Create UBO for config
    glCreateBuffers(1, &m_configUBO);
    glNamedBufferData(m_configUBO, 256, nullptr, GL_DYNAMIC_DRAW);  // Aligned to 256
    uploadConfigToGPU();

    // Build shaders
    if (!buildComputeShader()) {
        std::cerr << "[ParticleEmitter] Failed to build compute shader\n";
    }
    if (!buildRenderShader()) {
        std::cerr << "[ParticleEmitter] Failed to build render shader\n";
    }

    // Build billboard geometry
    buildBillboardGeometry();
    buildSpriteTexture();
    if (!m_config.spriteTexturePath.empty()) {
        loadSpriteTextureFromPath(m_config.spriteTexturePath);
    }

    // Initialize particles to zero
    resetParticles();
}

ParticleEmitter::~ParticleEmitter()
{
    if (m_particleSSBO) glDeleteBuffers(1, &m_particleSSBO);
    if (m_indirectDrawBuffer) glDeleteBuffers(1, &m_indirectDrawBuffer);
    if (m_configUBO) glDeleteBuffers(1, &m_configUBO);
    if (m_computeShader) glDeleteProgram(m_computeShader);
    if (m_renderShader) glDeleteProgram(m_renderShader);
    if (m_billboardVAO) glDeleteVertexArrays(1, &m_billboardVAO);
    if (m_spriteTexture) glDeleteTextures(1, &m_spriteTexture);
}

void ParticleEmitter::setConfig(const ParticleEmitterConfig& config)
{
    if (m_config.burstOnStart != config.burstOnStart && config.burstOnStart) {
        m_startBurstPending = true;
    }
    const bool spritePathChanged = m_config.spriteTexturePath != config.spriteTexturePath;
    m_config = config;
    uploadConfigToGPU();

    if (spritePathChanged) {
        if (!m_config.spriteTexturePath.empty()) {
            if (!loadSpriteTextureFromPath(m_config.spriteTexturePath)) {
                buildSpriteTexture();
            }
        } else {
            buildSpriteTexture();
        }
    }
}

void ParticleEmitter::resetParticles()
{
    std::vector<GPUParticle> particles(m_maxParticles);
    for (uint32_t i = 0; i < m_maxParticles; ++i) {
        particles[i].positionLife = glm::vec4(0.0f, 0.0f, 0.0f, -1.0f);  // Dead
        particles[i].velocityAge = glm::vec4(0.0f);
        particles[i].colorSize = glm::vec4(1.0f, 1.0f, 1.0f, 0.1f);
        particles[i].extraData = glm::vec4(0.0f, 0.0f, 1.0f, 0.0f);
    }
    glNamedBufferSubData(m_particleSSBO, 0, sizeof(GPUParticle) * m_maxParticles, particles.data());
}

void ParticleEmitter::uploadConfigToGPU()
{
    // Pack config into aligned structure for UBO
    struct GPUConfig {
        glm::vec4 position;
        glm::vec4 positionVariance;
        glm::vec4 velocity;
        glm::vec4 velocityVariance;
        glm::vec4 acceleration;
        glm::vec4 colorStart;
        glm::vec4 colorMid;
        glm::vec4 colorEnd;
        glm::vec4 scalar0;
        glm::vec4 scalar1;
        glm::vec4 scalar2;
        glm::vec4 scalar3;
        glm::vec4 scalar4;
        glm::vec4 scalar5;
    };
    
    GPUConfig gpuCfg{};
    gpuCfg.position = glm::vec4(m_config.position, 0.0f);
    gpuCfg.positionVariance = glm::vec4(m_config.positionVariance, 0.0f);
    gpuCfg.velocity = glm::vec4(m_config.velocity, 0.0f);
    gpuCfg.velocityVariance = glm::vec4(m_config.velocityVariance, 0.0f);
    gpuCfg.acceleration = glm::vec4(m_config.acceleration, 0.0f);
    gpuCfg.colorStart = m_config.colorStart;
    gpuCfg.colorMid = m_config.colorMid;
    gpuCfg.colorEnd = m_config.colorEnd;
    gpuCfg.scalar0 = glm::vec4(m_config.particleLifetime, m_config.emissionRate, m_config.damping, m_config.colorMidPoint);
    gpuCfg.scalar1 = glm::vec4(m_config.sizeStart, m_config.sizeMid, m_config.sizeEnd, m_config.sizeMidPoint);
    gpuCfg.scalar2 = glm::vec4(static_cast<float>(m_config.spriteColumns), static_cast<float>(m_config.spriteRows), m_config.spriteFrameRate, m_config.randomStartFrame ? 1.0f : 0.0f);
    gpuCfg.scalar3 = glm::vec4(m_config.collideWithGround ? 1.0f : 0.0f, m_config.bounceFactor, m_config.collisionFade, static_cast<float>(m_config.maxBounces));
    gpuCfg.scalar4 = glm::vec4(static_cast<float>(m_config.blendMode), 0.0f, 0.0f, 0.0f);
    gpuCfg.scalar5 = glm::vec4(m_config.collisionPlaneNormal, m_config.collisionPlaneOffset);
    
    glNamedBufferSubData(m_configUBO, 0, sizeof(GPUConfig), &gpuCfg);
}

bool ParticleEmitter::buildComputeShader()
{
    const char* computeSrc = R"(
#version 450 core

struct GPUParticle {
    vec4 positionLife;
    vec4 velocityAge;
    vec4 colorSize;
    vec4 extraData;
};

layout(std140, binding = 0) uniform ParticleConfig {
    vec4  config_position;
    vec4  config_positionVariance;
    vec4  config_velocity;
    vec4  config_velocityVariance;
    vec4  config_acceleration;
    vec4  config_colorStart;
    vec4  config_colorMid;
    vec4  config_colorEnd;
    vec4  config_scalar0;
    vec4  config_scalar1;
    vec4  config_scalar2;
    vec4  config_scalar3;
    vec4  config_scalar4;
    vec4  config_scalar5;
};

layout(std430, binding = 0) coherent buffer ParticleBuffer {
    GPUParticle particles[];
};

layout(local_size_x = 256) in;

uniform float u_dt;
uniform uint  u_maxParticles;

vec4 evalColor(float age) {
    float mid = clamp(config_scalar0.w, 0.001, 0.999);
    if (age < mid) {
        return mix(config_colorStart, config_colorMid, age / mid);
    }
    return mix(config_colorMid, config_colorEnd, (age - mid) / max(1.0 - mid, 0.001));
}

float evalSize(float age) {
    float mid = clamp(config_scalar1.w, 0.001, 0.999);
    if (age < mid) {
        return mix(config_scalar1.x, config_scalar1.y, age / mid);
    }
    return mix(config_scalar1.y, config_scalar1.z, (age - mid) / max(1.0 - mid, 0.001));
}

void main()
{
    uint idx = gl_GlobalInvocationID.x;
    if (idx >= u_maxParticles) return;

    GPUParticle particle = particles[idx];
    vec4 posLife = particle.positionLife;
    vec4 velAge  = particle.velocityAge;
    vec4 extra   = particle.extraData;

    // Lifetime countdown
    posLife.w -= u_dt;

    if (posLife.w <= 0.0f) {
        particles[idx].positionLife = vec4(posLife.xyz, -1.0);
        return;
    }

    // Position update
    vec3 pos = posLife.xyz;
    vec3 vel = velAge.xyz;
    float age = velAge.w;

    // Apply acceleration
    vel += config_acceleration.xyz * u_dt;
    vel *= config_scalar0.z;

    // Update position
    pos += vel * u_dt;

    vec3 planeNormal = normalize(config_scalar5.xyz);
    float planeOffset = config_scalar5.w;
    float planeDistance = dot(planeNormal, pos) - planeOffset;
    float velocityIntoPlane = dot(vel, planeNormal);

    if (config_scalar3.x > 0.5 && planeDistance <= 0.0 && velocityIntoPlane < 0.0) {
        if (extra.y < config_scalar3.w) {
            // Project back onto plane and bounce around the configurable plane normal.
            pos -= planeNormal * planeDistance;
            vec3 reflected = reflect(vel, planeNormal);
            vel = reflected * config_scalar3.y;
            vec3 tangent = vel - dot(vel, planeNormal) * planeNormal;
            tangent *= 0.82;
            vel = tangent + dot(vel, planeNormal) * planeNormal;
            extra.y += 1.0;
            extra.z *= max(1.0 - config_scalar3.z * 0.5, 0.0);
        } else {
            pos -= planeNormal * planeDistance;
            vel = vec3(0.0);
            extra.z *= max(1.0 - config_scalar3.z, 0.0);
            if (extra.z <= 0.02) {
                particles[idx].positionLife = vec4(pos, -1.0);
                particles[idx].velocityAge = vec4(0.0);
                particles[idx].colorSize = vec4(0.0);
                particles[idx].extraData = vec4(extra.x, extra.y, 0.0, extra.w);
                return;
            }
        }
    }

    // Update age (0..1)
    age = 1.0f - (posLife.w / config_scalar0.x);

    float frameCount = max(config_scalar2.x * config_scalar2.y, 1.0);
    float frame = extra.w;
    if (config_scalar2.z > 0.0) {
        frame = mod(floor(age * config_scalar2.z + extra.w), frameCount);
    }
    vec4 color = evalColor(age);
    float size = evalSize(age);

    particles[idx].positionLife = vec4(pos, posLife.w);
    particles[idx].velocityAge = vec4(vel, age);
    particles[idx].colorSize = vec4(color.rgb, size);
    particles[idx].extraData = vec4(frame, extra.y, extra.z * color.a, extra.w);
}
)";

    GLuint compute = glCreateShader(GL_COMPUTE_SHADER);
    glShaderSource(compute, 1, &computeSrc, nullptr);
    glCompileShader(compute);

    int success;
    glGetShaderiv(compute, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(compute, sizeof(infoLog), nullptr, infoLog);
        std::cerr << "[ParticleEmitter] Compute shader failed: " << infoLog << "\n";
        glDeleteShader(compute);
        return false;
    }

    m_computeShader = glCreateProgram();
    glAttachShader(m_computeShader, compute);
    glLinkProgram(m_computeShader);
    glDeleteShader(compute);

    glGetProgramiv(m_computeShader, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(m_computeShader, sizeof(infoLog), nullptr, infoLog);
        std::cerr << "[ParticleEmitter] Compute program link failed: " << infoLog << "\n";
        return false;
    }

    return true;
}

bool ParticleEmitter::buildRenderShader()
{
    const char* vertSrc = R"(
#version 450 core

struct GPUParticle {
    vec4 positionLife;
    vec4 velocityAge;
    vec4 colorSize;
    vec4 extraData;
};

out VS_OUT {
    vec4 color;
    float viewDepth;
    float frameIndex;
} vs_out;

layout(std430, binding = 0) buffer ParticleBuffer {
    GPUParticle particles[];
};

uniform mat4 u_viewProj;

void main()
{
    GPUParticle particle = particles[gl_InstanceID];
    if (particle.positionLife.w <= 0.0 || particle.extraData.z <= 0.001) {
        gl_Position = vec4(-2.0, -2.0, -2.0, 1.0);
        gl_PointSize = 0.0;
        vs_out.color = vec4(0.0);
        vs_out.viewDepth = 0.0;
        vs_out.frameIndex = 0.0;
        return;
    }
    vec4 clipPosition = u_viewProj * vec4(particle.positionLife.xyz, 1.0);
    gl_Position = clipPosition;
    gl_PointSize = max(particle.colorSize.w * 120.0, 1.0);
    vs_out.color = vec4(particle.colorSize.rgb, particle.extraData.z);
    vs_out.viewDepth = clipPosition.w;
    vs_out.frameIndex = particle.extraData.x;
}
)";

    const char* fragSrc = R"(
#version 450 core

in VS_OUT {
    vec4 color;
    float viewDepth;
    float frameIndex;
} fs_in;

uniform sampler2D u_ParticleTex;
uniform sampler2D u_SceneDepthTex;
uniform vec2 u_ViewportSize;
uniform float u_Near;
uniform float u_Far;
uniform float u_Softness;
uniform int u_SpriteColumns;
uniform int u_SpriteRows;

out vec4 FragColor;

float linearizeDepth(float depth)
{
    float z = depth * 2.0 - 1.0;
    return (2.0 * u_Near * u_Far) / (u_Far + u_Near - z * (u_Far - u_Near));
}

void main()
{
    vec2 center = gl_PointCoord - vec2(0.5);
    float dist = length(center);
    if (dist > 0.5) discard;

    int cols = max(u_SpriteColumns, 1);
    int rows = max(u_SpriteRows, 1);
    int frameCount = max(cols * rows, 1);
    int frame = clamp(int(fs_in.frameIndex + 0.5), 0, frameCount - 1);
    vec2 cell = vec2(frame % cols, frame / cols);
    vec2 atlasUV = (cell + gl_PointCoord) / vec2(cols, rows);
    vec4 sprite = texture(u_ParticleTex, atlasUV);
    float particleDepth = linearizeDepth(gl_FragCoord.z);
    float sceneDepth = linearizeDepth(texture(u_SceneDepthTex, gl_FragCoord.xy / u_ViewportSize).r);
    float softFade = 1.0;
    if (sceneDepth < u_Far * 0.999) {
        softFade = clamp((sceneDepth - particleDepth) / max(u_Softness, 0.0001), 0.0, 1.0);
    }

    float radial = 1.0 - dist * 2.0;
    float alpha = radial * sprite.a * fs_in.color.a * softFade;
    FragColor = vec4(fs_in.color.rgb * sprite.rgb, alpha);
}
)";

    GLuint vert = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vert, 1, &vertSrc, nullptr);
    glCompileShader(vert);

    int success;
    glGetShaderiv(vert, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(vert, sizeof(infoLog), nullptr, infoLog);
        std::cerr << "[ParticleEmitter] Render vertex shader failed: " << infoLog << "\n";
        glDeleteShader(vert);
        return false;
    }

    GLuint frag = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(frag, 1, &fragSrc, nullptr);
    glCompileShader(frag);

    glGetShaderiv(frag, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(frag, sizeof(infoLog), nullptr, infoLog);
        std::cerr << "[ParticleEmitter] Render fragment shader failed: " << infoLog << "\n";
        glDeleteShader(vert);
        glDeleteShader(frag);
        return false;
    }

    m_renderShader = glCreateProgram();
    glAttachShader(m_renderShader, vert);
    glAttachShader(m_renderShader, frag);
    glLinkProgram(m_renderShader);
    glDeleteShader(vert);
    glDeleteShader(frag);

    glGetProgramiv(m_renderShader, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(m_renderShader, sizeof(infoLog), nullptr, infoLog);
        std::cerr << "[ParticleEmitter] Render program link failed: " << infoLog << "\n";
        return false;
    }

    return true;
}

void ParticleEmitter::buildBillboardGeometry()
{
    glCreateVertexArrays(1, &m_billboardVAO);
}

void ParticleEmitter::buildSpriteTexture()
{
    constexpr int kCellSize = 64;
    constexpr int kCols = 8;
    constexpr int kRows = 8;
    constexpr int kWidth = kCellSize * kCols;
    constexpr int kHeight = kCellSize * kRows;
    std::array<unsigned char, kWidth * kHeight * 4> pixels{};

    for (int frameY = 0; frameY < kRows; ++frameY) {
        for (int frameX = 0; frameX < kCols; ++frameX) {
            const int frameIndex = frameY * kCols + frameX;
            const float phase = static_cast<float>(frameIndex) / static_cast<float>(kCols * kRows - 1);
            for (int y = 0; y < kCellSize; ++y) {
                for (int x = 0; x < kCellSize; ++x) {
                    const float u = (static_cast<float>(x) + 0.5f) / static_cast<float>(kCellSize);
                    const float v = (static_cast<float>(y) + 0.5f) / static_cast<float>(kCellSize);
                    const float dx = u - 0.5f;
                    const float dy = v - 0.5f;
                    const float dist = std::sqrt(dx * dx + dy * dy) * 2.0f;
                    const float angle = std::atan2(dy, dx);
                    const float swirl = 0.5f + 0.5f * std::sin(angle * (3.0f + phase * 5.0f) + phase * 9.0f);
                    const float falloff = glm::clamp(1.0f - dist, 0.0f, 1.0f);
                    const float ring = 0.45f + 0.55f * std::cos(dist * (10.0f + phase * 22.0f));
                    const float alpha = glm::clamp(falloff * falloff * (0.55f + 0.45f * swirl) * (0.65f + 0.35f * ring), 0.0f, 1.0f);

                    const int outX = frameX * kCellSize + x;
                    const int outY = frameY * kCellSize + y;
                    const std::size_t pixelIndex = static_cast<std::size_t>((outY * kWidth + outX) * 4);
                    pixels[pixelIndex + 0] = static_cast<unsigned char>(255.0f * glm::clamp(0.78f + 0.22f * swirl, 0.0f, 1.0f));
                    pixels[pixelIndex + 1] = static_cast<unsigned char>(255.0f * glm::clamp(0.74f + 0.18f * (1.0f - phase), 0.0f, 1.0f));
                    pixels[pixelIndex + 2] = static_cast<unsigned char>(255.0f);
                    pixels[pixelIndex + 3] = static_cast<unsigned char>(255.0f * alpha);
                }
            }
        }
    }

    if (m_spriteTexture) {
        glDeleteTextures(1, &m_spriteTexture);
        m_spriteTexture = 0;
    }
    glCreateTextures(GL_TEXTURE_2D, 1, &m_spriteTexture);
    glTextureStorage2D(m_spriteTexture, 1, GL_RGBA8, kWidth, kHeight);
    glTextureSubImage2D(m_spriteTexture, 0, 0, 0, kWidth, kHeight, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    glTextureParameteri(m_spriteTexture, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTextureParameteri(m_spriteTexture, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTextureParameteri(m_spriteTexture, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(m_spriteTexture, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    m_loadedSpriteTexturePath.clear();
}

bool ParticleEmitter::loadSpriteTextureFromPath(const std::string& texturePath)
{
    if (texturePath.empty()) {
        return false;
    }

    std::filesystem::path filePath{texturePath};
    if (filePath.is_relative()) {
        filePath = std::filesystem::current_path() / filePath;
    }
    filePath = filePath.lexically_normal();

    int width = 0;
    int height = 0;
    int channels = 0;
    unsigned char* pixels = stbi_load(filePath.string().c_str(), &width, &height, &channels, STBI_rgb_alpha);
    if (pixels == nullptr || width <= 0 || height <= 0) {
        std::cerr << "[ParticleEmitter] Failed to load sprite texture: " << filePath.string() << "\n";
        if (pixels != nullptr) {
            stbi_image_free(pixels);
        }
        return false;
    }

    if (m_spriteTexture) {
        glDeleteTextures(1, &m_spriteTexture);
        m_spriteTexture = 0;
    }
    glCreateTextures(GL_TEXTURE_2D, 1, &m_spriteTexture);
    glTextureStorage2D(m_spriteTexture, 1, GL_RGBA8, width, height);
    glTextureSubImage2D(m_spriteTexture, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    glTextureParameteri(m_spriteTexture, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTextureParameteri(m_spriteTexture, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTextureParameteri(m_spriteTexture, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(m_spriteTexture, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    stbi_image_free(pixels);

    m_loadedSpriteTexturePath = texturePath;
    return true;
}

void ParticleEmitter::update(float dt)
{
    if (!m_config.enabled || dt <= 0.0f) return;

    uint32_t toEmit = 0;

    if (m_startBurstPending) {
        if (m_config.burstOnStart && m_config.burstCount > 0U) {
            toEmit += m_config.burstCount;
        }
        m_startBurstPending = false;
        m_burstAccumulator = 0.0f;
    }

    if (m_config.looping && m_config.burstCount > 0U && m_config.burstInterval > 0.0f) {
        m_burstAccumulator += dt;
        while (m_burstAccumulator >= m_config.burstInterval) {
            toEmit += m_config.burstCount;
            m_burstAccumulator -= m_config.burstInterval;
        }
    }

    m_emissionAccumulator += m_config.emissionRate * dt;
    const uint32_t continuousToEmit = static_cast<uint32_t>(m_emissionAccumulator);
    toEmit += continuousToEmit;
    m_emissionAccumulator -= static_cast<float>(continuousToEmit);

    if (toEmit > 0 || m_activeCount > 0) {
        auto* particles = static_cast<GPUParticle*>(
            glMapNamedBufferRange(
                m_particleSSBO,
                0,
                sizeof(GPUParticle) * m_maxParticles,
                GL_MAP_READ_BIT | GL_MAP_WRITE_BIT)
        );

        if (particles) {
            uint32_t liveCount = 0;
            uint32_t emittedCount = 0;
            for (uint32_t idx = 0; idx < m_maxParticles; ++idx) {
                GPUParticle& p = particles[idx];
                if (p.positionLife.w > 0.0f) {
                    ++liveCount;
                    continue;
                }
                if (emittedCount >= toEmit) {
                    continue;
                }

                glm::vec3 pos = m_config.position;
                pos.x += glm::linearRand(-m_config.positionVariance.x, m_config.positionVariance.x);
                pos.y += glm::linearRand(-m_config.positionVariance.y, m_config.positionVariance.y);
                pos.z += glm::linearRand(-m_config.positionVariance.z, m_config.positionVariance.z);

                glm::vec3 vel = m_config.velocity;
                vel.x += glm::linearRand(-m_config.velocityVariance.x, m_config.velocityVariance.x);
                vel.y += glm::linearRand(-m_config.velocityVariance.y, m_config.velocityVariance.y);
                vel.z += glm::linearRand(-m_config.velocityVariance.z, m_config.velocityVariance.z);

                p.positionLife = glm::vec4(pos, m_config.particleLifetime);
                p.velocityAge = glm::vec4(vel, 0.0f);
                p.colorSize = glm::vec4(glm::vec3(m_config.colorStart), m_config.sizeStart);
                const int frameCount = std::max(m_config.spriteColumns * m_config.spriteRows, 1);
                const float frameOffset = m_config.randomStartFrame
                    ? static_cast<float>(glm::linearRand(0, frameCount - 1))
                    : 0.0f;
                p.extraData = glm::vec4(frameOffset, 0.0f, 1.0f, frameOffset);
                ++emittedCount;
            }

            glUnmapNamedBuffer(m_particleSSBO);
            m_activeCount = liveCount + emittedCount;
        }
    }

    // Run compute shader
    if (m_computeShader) {
        glUseProgram(m_computeShader);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, m_particleSSBO);
        glBindBufferBase(GL_UNIFORM_BUFFER, 0, m_configUBO);
        glUniform1f(glGetUniformLocation(m_computeShader, "u_dt"), dt);
        glUniform1ui(glGetUniformLocation(m_computeShader, "u_maxParticles"), m_maxParticles);
        
        uint32_t groupCount = (m_maxParticles + 255) / 256;
        glDispatchCompute(groupCount, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT);

        if (auto* particles = static_cast<const GPUParticle*>(
                glMapNamedBufferRange(m_particleSSBO, 0, sizeof(GPUParticle) * m_maxParticles, GL_MAP_READ_BIT))) {
            uint32_t liveCount = 0;
            for (uint32_t idx = 0; idx < m_maxParticles; ++idx) {
                if (particles[idx].positionLife.w > 0.0f && particles[idx].extraData.z > 0.001f) {
                    ++liveCount;
                }
            }
            glUnmapNamedBuffer(m_particleSSBO);
            m_activeCount = liveCount;
        }
    }
}

void ParticleEmitter::render(const glm::mat4& viewProj,
                             GLuint depthTexture,
                             const glm::vec2& viewportSize,
                             float nearPlane,
                             float farPlane) const
{
    if (!m_renderShader || !m_billboardVAO) return;
    if (m_activeCount == 0) return;

    glEnable(GL_BLEND);
    switch (m_config.blendMode) {
        case ParticleEmitterConfig::BlendMode::Alpha:
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            break;
        case ParticleEmitterConfig::BlendMode::Additive:
            glBlendFunc(GL_SRC_ALPHA, GL_ONE);
            break;
        case ParticleEmitterConfig::BlendMode::Premultiplied:
            glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
            break;
    }
    glEnable(GL_PROGRAM_POINT_SIZE);
    glDepthMask(GL_FALSE);

    glUseProgram(m_renderShader);
    glUniformMatrix4fv(glGetUniformLocation(m_renderShader, "u_viewProj"), 1, GL_FALSE, glm::value_ptr(viewProj));
    glUniform2fv(glGetUniformLocation(m_renderShader, "u_ViewportSize"), 1, glm::value_ptr(viewportSize));
    glUniform1f(glGetUniformLocation(m_renderShader, "u_Near"), nearPlane);
    glUniform1f(glGetUniformLocation(m_renderShader, "u_Far"), farPlane);
    glUniform1f(glGetUniformLocation(m_renderShader, "u_Softness"), 0.75f);
    glUniform1i(glGetUniformLocation(m_renderShader, "u_SpriteColumns"), std::max(m_config.spriteColumns, 1));
    glUniform1i(glGetUniformLocation(m_renderShader, "u_SpriteRows"), std::max(m_config.spriteRows, 1));
    glBindTextureUnit(0, m_spriteTexture);
    glBindTextureUnit(1, depthTexture);
    glUniform1i(glGetUniformLocation(m_renderShader, "u_ParticleTex"), 0);
    glUniform1i(glGetUniformLocation(m_renderShader, "u_SceneDepthTex"), 1);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, m_particleSSBO);
    glBindVertexArray(m_billboardVAO);

    glDrawArraysInstanced(GL_POINTS, 0, 1, static_cast<GLsizei>(m_activeCount));

    glDepthMask(GL_TRUE);
    glDisable(GL_PROGRAM_POINT_SIZE);
    glDisable(GL_BLEND);
}

// ============================================================================
// PARTICLE SYSTEM (Singleton)
// ============================================================================

ParticleSystem& ParticleSystem::instance()
{
    static ParticleSystem inst;
    return inst;
}

void ParticleSystem::update(float dt)
{
    for (auto& emitter : m_emitters) {
        if (emitter) emitter->update(dt);
    }
}

void ParticleSystem::render(const glm::mat4& viewProj,
                            GLuint depthTexture,
                            const glm::vec2& viewportSize,
                            float nearPlane,
                            float farPlane)
{
    for (auto& emitter : m_emitters) {
        if (emitter) emitter->render(viewProj, depthTexture, viewportSize, nearPlane, farPlane);
    }
}

std::shared_ptr<ParticleEmitter> ParticleSystem::createEmitter(const ParticleEmitterConfig& config, uint32_t maxParticles)
{
    auto emitter = std::make_shared<ParticleEmitter>(config, maxParticles);
    m_emitters.push_back(emitter);
    return emitter;
}

void ParticleSystem::destroyEmitter(std::shared_ptr<ParticleEmitter> emitter)
{
    auto it = std::find(m_emitters.begin(), m_emitters.end(), emitter);
    if (it != m_emitters.end()) {
        m_emitters.erase(it);
    }
}

} // namespace elysium
