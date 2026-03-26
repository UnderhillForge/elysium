#include "Application.hpp"

#include "Editor/Theme.hpp"

#include <SDL.h>
#include <glad/glad.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <backends/imgui_impl_opengl3.h>
#include <backends/imgui_impl_sdl2.h>
#include <spdlog/spdlog.h>
#include <stb_image.h>
#include <json.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstring>
#include <fstream>
#include <limits>
#include <map>
#include <sstream>
#include <unordered_map>

namespace elysium {

namespace {

using json = nlohmann::json;

glm::vec3 gizmoAxisVector(Application::GizmoAxis axis) {
    switch (axis) {
        case Application::GizmoAxis::X: return glm::vec3(1.0f, 0.0f, 0.0f);
        case Application::GizmoAxis::Y: return glm::vec3(0.0f, 1.0f, 0.0f);
        case Application::GizmoAxis::Z: return glm::vec3(0.0f, 0.0f, 1.0f);
        case Application::GizmoAxis::None: break;
    }
    return glm::vec3(0.0f);
}

ImU32 gizmoAxisColor(Application::GizmoAxis axis, bool active) {
    switch (axis) {
        case Application::GizmoAxis::X: return active ? IM_COL32(255, 120, 120, 255) : IM_COL32(224, 70, 70, 255);
        case Application::GizmoAxis::Y: return active ? IM_COL32(140, 255, 160, 255) : IM_COL32(70, 210, 90, 255);
        case Application::GizmoAxis::Z: return active ? IM_COL32(120, 180, 255, 255) : IM_COL32(70, 120, 224, 255);
        case Application::GizmoAxis::None: break;
    }
    return IM_COL32(220, 220, 220, 255);
}

float distanceToSegment(const ImVec2& p, const ImVec2& a, const ImVec2& b) {
    const float abx = b.x - a.x;
    const float aby = b.y - a.y;
    const float apx = p.x - a.x;
    const float apy = p.y - a.y;
    const float abLenSq = abx * abx + aby * aby;
    if (abLenSq <= 0.0001f) {
        const float dx = p.x - a.x;
        const float dy = p.y - a.y;
        return std::sqrt(dx * dx + dy * dy);
    }

    const float t = std::clamp((apx * abx + apy * aby) / abLenSq, 0.0f, 1.0f);
    const float cx = a.x + abx * t;
    const float cy = a.y + aby * t;
    const float dx = p.x - cx;
    const float dy = p.y - cy;
    return std::sqrt(dx * dx + dy * dy);
}

const char* gizmoModeLabel(Application::GizmoMode mode) {
    switch (mode) {
        case Application::GizmoMode::Translate: return "Translate";
        case Application::GizmoMode::Rotate: return "Rotate";
        case Application::GizmoMode::Scale: return "Scale";
    }
    return "Unknown";
}

const char* dockPresetLabel(Application::DockPreset preset) {
    switch (preset) {
        case Application::DockPreset::Default: return "Default";
        case Application::DockPreset::Terrain: return "Terrain";
        case Application::DockPreset::AssetPrep: return "Asset Prep";
    }
    return "Default";
}

const char* particlePresetLabel(SceneEntity::ParticleEmitterComponent::Preset preset) {
    switch (preset) {
        case SceneEntity::ParticleEmitterComponent::Preset::Custom: return "Custom";
        case SceneEntity::ParticleEmitterComponent::Preset::Fire: return "Fire";
        case SceneEntity::ParticleEmitterComponent::Preset::Smoke: return "Smoke";
        case SceneEntity::ParticleEmitterComponent::Preset::Sparks: return "Sparks";
        case SceneEntity::ParticleEmitterComponent::Preset::Magic: return "Magic";
    }
    return "Custom";
}

const char* particleBlendModeLabel(SceneEntity::ParticleEmitterComponent::BlendMode blendMode) {
    switch (blendMode) {
        case SceneEntity::ParticleEmitterComponent::BlendMode::Alpha: return "Alpha";
        case SceneEntity::ParticleEmitterComponent::BlendMode::Additive: return "Additive";
        case SceneEntity::ParticleEmitterComponent::BlendMode::Premultiplied: return "Premultiplied";
    }
    return "Alpha";
}

const char* assetTypeLabel(AssetType type) {
    switch (type) {
        case AssetType::Model: return "Models";
        case AssetType::Texture: return "Textures";
        case AssetType::Script: return "Scripts";
        case AssetType::Audio: return "Audio";
        case AssetType::Material: return "Materials";
        case AssetType::Shader: return "Shaders";
        case AssetType::Other: break;
    }
    return "Other";
}

const char* assetTypeBadge(AssetType type) {
    switch (type) {
        case AssetType::Model: return "3D";
        case AssetType::Texture: return "TEX";
        case AssetType::Script: return "SCR";
        case AssetType::Audio: return "AUD";
        case AssetType::Material: return "MAT";
        case AssetType::Shader: return "SHD";
        case AssetType::Other: break;
    }
    return "AST";
}

ImU32 assetTypeColor(AssetType type) {
    switch (type) {
        case AssetType::Model: return IM_COL32(100, 170, 255, 255);
        case AssetType::Texture: return IM_COL32(122, 215, 132, 255);
        case AssetType::Script: return IM_COL32(255, 197, 93, 255);
        case AssetType::Audio: return IM_COL32(202, 146, 255, 255);
        case AssetType::Material: return IM_COL32(255, 145, 110, 255);
        case AssetType::Shader: return IM_COL32(98, 236, 220, 255);
        case AssetType::Other: break;
    }
    return IM_COL32(180, 180, 180, 255);
}

const char* materialDebugViewLabel(EditorViewport::MaterialDebugView view) {
    switch (view) {
        case EditorViewport::MaterialDebugView::Lit: return "Lit";
        case EditorViewport::MaterialDebugView::Normals: return "Normals";
        case EditorViewport::MaterialDebugView::Roughness: return "Roughness";
        case EditorViewport::MaterialDebugView::Metallic: return "Metallic";
        case EditorViewport::MaterialDebugView::Emissive: return "Emissive";
    }
    return "Unknown";
}

std::string assetCategoryFromRelativePath(const std::string& relativePath) {
    const std::filesystem::path rel{relativePath};
    const std::filesystem::path parent = rel.parent_path();
    if (parent.empty()) {
        return "root";
    }

    const std::string parentString = parent.generic_string();
    const std::size_t slashPos = parentString.find('/');
    if (slashPos == std::string::npos) {
        return parentString;
    }
    return parentString.substr(0, slashPos);
}

std::string lowercaseExtension(const std::filesystem::path& path) {
    std::string ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return ext;
}

bool isRuntimeModelFormat(const std::filesystem::path& path) {
    const std::string ext = lowercaseExtension(path);
    return ext == ".gltf" || ext == ".glb";
}

bool isSourceModelFormat(const std::filesystem::path& path) {
    const std::string ext = lowercaseExtension(path);
    return ext == ".fbx" || ext == ".obj";
}

bool isKnownAudioFormat(const std::filesystem::path& path) {
    const std::string ext = lowercaseExtension(path);
    return ext == ".wav" || ext == ".ogg" || ext == ".mp3" || ext == ".flac";
}

bool isWavAudioFormat(const std::filesystem::path& path) {
    return lowercaseExtension(path) == ".wav";
}

bool buildThumbnailPixelsRGBA(const unsigned char* src,
                             int srcW,
                             int srcH,
                             int thumbEdge,
                             std::vector<unsigned char>& outPixels,
                             int& outW,
                             int& outH)
{
    if (src == nullptr || srcW <= 0 || srcH <= 0 || thumbEdge <= 0) {
        return false;
    }

    const int maxDim = std::max(srcW, srcH);
    if (maxDim <= thumbEdge) {
        outW = srcW;
        outH = srcH;
        outPixels.assign(src, src + static_cast<std::size_t>(srcW) * static_cast<std::size_t>(srcH) * 4U);
        return true;
    }

    const float scale = static_cast<float>(thumbEdge) / static_cast<float>(maxDim);
    outW = std::max(1, static_cast<int>(std::round(static_cast<float>(srcW) * scale)));
    outH = std::max(1, static_cast<int>(std::round(static_cast<float>(srcH) * scale)));
    outPixels.resize(static_cast<std::size_t>(outW) * static_cast<std::size_t>(outH) * 4U);

    for (int y = 0; y < outH; ++y) {
        for (int x = 0; x < outW; ++x) {
            const int srcX = std::clamp(static_cast<int>(std::floor((static_cast<float>(x) / static_cast<float>(outW)) * static_cast<float>(srcW))), 0, srcW - 1);
            const int srcY = std::clamp(static_cast<int>(std::floor((static_cast<float>(y) / static_cast<float>(outH)) * static_cast<float>(srcH))), 0, srcH - 1);
            const std::size_t srcIdx = (static_cast<std::size_t>(srcY) * static_cast<std::size_t>(srcW) + static_cast<std::size_t>(srcX)) * 4U;
            const std::size_t dstIdx = (static_cast<std::size_t>(y) * static_cast<std::size_t>(outW) + static_cast<std::size_t>(x)) * 4U;
            outPixels[dstIdx + 0U] = src[srcIdx + 0U];
            outPixels[dstIdx + 1U] = src[srcIdx + 1U];
            outPixels[dstIdx + 2U] = src[srcIdx + 2U];
            outPixels[dstIdx + 3U] = src[srcIdx + 3U];
        }
    }

    return true;
}

bool createTextureThumbnailFromFile(const std::filesystem::path& filePath,
                                    GLuint& outTexture)
{
    constexpr int kThumbEdge = 96;

    int srcW = 0;
    int srcH = 0;
    int srcChannels = 0;
    unsigned char* srcPixels = stbi_load(filePath.string().c_str(), &srcW, &srcH, &srcChannels, STBI_rgb_alpha);
    if (srcPixels == nullptr || srcW <= 0 || srcH <= 0) {
        if (srcPixels != nullptr) {
            stbi_image_free(srcPixels);
        }
        return false;
    }

    std::vector<unsigned char> thumbPixels;
    int thumbW = 0;
    int thumbH = 0;
    const bool ok = buildThumbnailPixelsRGBA(srcPixels, srcW, srcH, kThumbEdge, thumbPixels, thumbW, thumbH);
    stbi_image_free(srcPixels);
    if (!ok || thumbW <= 0 || thumbH <= 0 || thumbPixels.empty()) {
        return false;
    }

    glCreateTextures(GL_TEXTURE_2D, 1, &outTexture);
    glTextureStorage2D(outTexture, 1, GL_RGBA8, thumbW, thumbH);
    glTextureSubImage2D(outTexture, 0, 0, 0, thumbW, thumbH, GL_RGBA, GL_UNSIGNED_BYTE, thumbPixels.data());
    glTextureParameteri(outTexture, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTextureParameteri(outTexture, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTextureParameteri(outTexture, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(outTexture, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    return true;
}

bool createTextureThumbnailFromGLTexture(GLuint sourceTexture,
                                         GLuint& outTexture)
{
    if (sourceTexture == 0U) {
        return false;
    }

    GLint srcW = 0;
    GLint srcH = 0;
    glGetTextureLevelParameteriv(sourceTexture, 0, GL_TEXTURE_WIDTH, &srcW);
    glGetTextureLevelParameteriv(sourceTexture, 0, GL_TEXTURE_HEIGHT, &srcH);
    if (srcW <= 0 || srcH <= 0) {
        return false;
    }

    std::vector<unsigned char> srcPixels(static_cast<std::size_t>(srcW) * static_cast<std::size_t>(srcH) * 4U);
    glGetTextureImage(sourceTexture, 0, GL_RGBA, GL_UNSIGNED_BYTE, static_cast<GLsizei>(srcPixels.size()), srcPixels.data());

    constexpr int kThumbEdge = 96;
    std::vector<unsigned char> thumbPixels;
    int thumbW = 0;
    int thumbH = 0;
    if (!buildThumbnailPixelsRGBA(srcPixels.data(), srcW, srcH, kThumbEdge, thumbPixels, thumbW, thumbH)) {
        return false;
    }

    glCreateTextures(GL_TEXTURE_2D, 1, &outTexture);
    glTextureStorage2D(outTexture, 1, GL_RGBA8, thumbW, thumbH);
    glTextureSubImage2D(outTexture, 0, 0, 0, thumbW, thumbH, GL_RGBA, GL_UNSIGNED_BYTE, thumbPixels.data());
    glTextureParameteri(outTexture, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTextureParameteri(outTexture, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTextureParameteri(outTexture, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(outTexture, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    return true;
}

bool createModelThumbnail(const AssetEntry& entry,
                          GLTFLoader& loader,
                          std::unordered_map<std::string, std::shared_ptr<GLTFModel>>& modelCache,
                          bool allowModelLoad,
                          GLuint& outTexture)
{
    std::unordered_map<std::string, std::shared_ptr<GLTFModel>>::iterator it = modelCache.end();
    if (allowModelLoad && isRuntimeModelFormat(entry.absolutePath)) {
        const std::string modelKey = entry.absolutePath.lexically_normal().string();
        it = modelCache.find(modelKey);
        if (it == modelCache.end()) {
            const auto loaded = loader.loadModelFromFile(entry.absolutePath);
            if (loaded) {
                it = modelCache.emplace(modelKey, loaded).first;
            }
        }
    }

    if (it != modelCache.end() && it->second) {
        for (const auto& material : it->second->materials) {
            if (material.hasBaseColorTexture && material.baseColorTexture != 0U) {
                if (createTextureThumbnailFromGLTexture(material.baseColorTexture, outTexture)) {
                    return true;
                }
            }
        }
    }

    const std::filesystem::path stemPath = entry.absolutePath.parent_path() / entry.absolutePath.stem();
    for (const char* ext : {".png", ".jpg", ".jpeg"}) {
        const std::filesystem::path sidecar = stemPath;
        const std::filesystem::path candidate = sidecar.string() + ext;
        if (std::filesystem::exists(candidate)) {
            if (createTextureThumbnailFromFile(candidate, outTexture)) {
                return true;
            }
        }
    }

    return false;
}

struct CollisionPlaneOverlay {
    glm::vec3 planeNormal{0.0f, 1.0f, 0.0f};
    glm::vec3 planeCenter{0.0f};
    float planeHalfExtent{1.0f};
    std::array<ImVec2, 4> screenCorners{};
    bool cornersProjected{false};
    ImVec2 centerScreen{0.0f, 0.0f};
    ImVec2 tipScreen{0.0f, 0.0f};
    bool centerProjected{false};
    bool tipProjected{false};
};

bool buildCollisionPlaneOverlay(const SceneEntity& entity,
                                const EditorViewport& viewport,
                                const float gizmoLength,
                                const ImVec2& imageMin,
                                const ImVec2& imageMax,
                                CollisionPlaneOverlay& out)
{
    if (!entity.particleEmitter.has_value() || !entity.particleEmitter->collideWithGround) {
        return false;
    }

    const auto& pe = *entity.particleEmitter;
    out.planeNormal = pe.collisionPlaneNormal;
    const float planeNormalLenSq = glm::dot(out.planeNormal, out.planeNormal);
    if (planeNormalLenSq <= 1e-4f) {
        out.planeNormal = glm::vec3(0.0f, 1.0f, 0.0f);
    } else {
        out.planeNormal = glm::normalize(out.planeNormal);
    }

    const float signedDistance = glm::dot(out.planeNormal, entity.transform.position) - pe.collisionPlaneOffset;
    out.planeCenter = entity.transform.position - out.planeNormal * signedDistance;

    const glm::vec3 refAxis = std::abs(out.planeNormal.y) < 0.95f
        ? glm::vec3(0.0f, 1.0f, 0.0f)
        : glm::vec3(1.0f, 0.0f, 0.0f);
    const glm::vec3 tangent = glm::normalize(glm::cross(refAxis, out.planeNormal));
    const glm::vec3 bitangent = glm::normalize(glm::cross(out.planeNormal, tangent));
    out.planeHalfExtent = std::max(0.75f, gizmoLength * 0.95f);

    const std::array<glm::vec3, 4> corners = {
        out.planeCenter + tangent * out.planeHalfExtent + bitangent * out.planeHalfExtent,
        out.planeCenter - tangent * out.planeHalfExtent + bitangent * out.planeHalfExtent,
        out.planeCenter - tangent * out.planeHalfExtent - bitangent * out.planeHalfExtent,
        out.planeCenter + tangent * out.planeHalfExtent - bitangent * out.planeHalfExtent
    };

    out.cornersProjected = true;
    for (std::size_t i = 0; i < corners.size(); ++i) {
        if (!viewport.worldToScreen(corners[i], imageMin, imageMax, out.screenCorners[i])) {
            out.cornersProjected = false;
            break;
        }
    }

    out.centerProjected = viewport.worldToScreen(out.planeCenter, imageMin, imageMax, out.centerScreen);
    out.tipProjected = viewport.worldToScreen(out.planeCenter + out.planeNormal * (out.planeHalfExtent * 0.85f), imageMin, imageMax, out.tipScreen);
    return true;
}

void applyParticleEmitterPreset(SceneEntity::ParticleEmitterComponent& emitter) {
    using Preset = SceneEntity::ParticleEmitterComponent::Preset;

    switch (emitter.preset) {
        case Preset::Custom:
            return;
        case Preset::Fire:
            emitter.blendMode = SceneEntity::ParticleEmitterComponent::BlendMode::Additive;
            emitter.maxParticles = 12000;
            emitter.emissionRate = 220.0f;
            emitter.particleLifetime = 1.2f;
            emitter.velocity = glm::vec3(0.0f, 2.4f, 0.0f);
            emitter.velocityVariance = glm::vec3(0.35f, 0.9f, 0.35f);
            emitter.positionVariance = glm::vec3(0.18f, 0.05f, 0.18f);
            emitter.acceleration = glm::vec3(0.0f, 0.45f, 0.0f);
            emitter.damping = 0.97f;
            emitter.looping = true;
            emitter.burstCount = 0;
            emitter.burstInterval = 1.0f;
            emitter.burstOnStart = false;
            emitter.colorStart = glm::vec4(1.0f, 0.72f, 0.22f, 1.0f);
            emitter.colorMid = glm::vec4(1.0f, 0.35f, 0.08f, 0.75f);
            emitter.colorEnd = glm::vec4(0.55f, 0.12f, 0.02f, 0.0f);
            emitter.colorMidPoint = 0.28f;
            emitter.sizeStart = 0.16f;
            emitter.sizeMid = 0.11f;
            emitter.sizeEnd = 0.05f;
            emitter.sizeMidPoint = 0.30f;
            emitter.spriteColumns = 4;
            emitter.spriteRows = 4;
            emitter.spriteFrameRate = 18.0f;
            emitter.randomStartFrame = true;
            emitter.spriteTexturePath = "assets/textures/particles/fire_sheet.png";
            emitter.collideWithGround = false;
            emitter.collisionPlaneNormal = glm::vec3(0.0f, 1.0f, 0.0f);
            emitter.collisionPlaneOffset = 0.0f;
            emitter.bounceFactor = 0.0f;
            emitter.collisionFade = 0.0f;
            emitter.maxBounces = 0;
            return;
        case Preset::Smoke:
            emitter.blendMode = SceneEntity::ParticleEmitterComponent::BlendMode::Alpha;
            emitter.maxParticles = 8000;
            emitter.emissionRate = 90.0f;
            emitter.particleLifetime = 3.8f;
            emitter.velocity = glm::vec3(0.0f, 0.9f, 0.0f);
            emitter.velocityVariance = glm::vec3(0.25f, 0.35f, 0.25f);
            emitter.positionVariance = glm::vec3(0.2f, 0.05f, 0.2f);
            emitter.acceleration = glm::vec3(0.0f, 0.12f, 0.0f);
            emitter.damping = 0.992f;
            emitter.looping = true;
            emitter.burstCount = 0;
            emitter.burstInterval = 1.0f;
            emitter.burstOnStart = false;
            emitter.colorStart = glm::vec4(0.45f, 0.45f, 0.45f, 0.75f);
            emitter.colorMid = glm::vec4(0.28f, 0.28f, 0.30f, 0.35f);
            emitter.colorEnd = glm::vec4(0.12f, 0.12f, 0.12f, 0.0f);
            emitter.colorMidPoint = 0.55f;
            emitter.sizeStart = 0.22f;
            emitter.sizeMid = 0.34f;
            emitter.sizeEnd = 0.42f;
            emitter.sizeMidPoint = 0.55f;
            emitter.spriteColumns = 4;
            emitter.spriteRows = 4;
            emitter.spriteFrameRate = 10.0f;
            emitter.randomStartFrame = true;
            emitter.spriteTexturePath = "assets/textures/particles/smoke_sheet.png";
            emitter.collideWithGround = false;
            emitter.collisionPlaneNormal = glm::vec3(0.0f, 1.0f, 0.0f);
            emitter.collisionPlaneOffset = 0.0f;
            emitter.bounceFactor = 0.0f;
            emitter.collisionFade = 0.0f;
            emitter.maxBounces = 0;
            return;
        case Preset::Sparks:
            emitter.blendMode = SceneEntity::ParticleEmitterComponent::BlendMode::Additive;
            emitter.maxParticles = 4096;
            emitter.emissionRate = 0.0f;
            emitter.particleLifetime = 0.9f;
            emitter.velocity = glm::vec3(0.0f, 2.6f, 0.0f);
            emitter.velocityVariance = glm::vec3(2.0f, 1.4f, 2.0f);
            emitter.positionVariance = glm::vec3(0.05f, 0.05f, 0.05f);
            emitter.acceleration = glm::vec3(0.0f, -4.0f, 0.0f);
            emitter.damping = 0.985f;
            emitter.looping = true;
            emitter.burstCount = 48;
            emitter.burstInterval = 0.18f;
            emitter.burstOnStart = true;
            emitter.colorStart = glm::vec4(1.0f, 0.92f, 0.55f, 1.0f);
            emitter.colorMid = glm::vec4(1.0f, 0.55f, 0.12f, 0.65f);
            emitter.colorEnd = glm::vec4(1.0f, 0.28f, 0.05f, 0.0f);
            emitter.colorMidPoint = 0.20f;
            emitter.sizeStart = 0.08f;
            emitter.sizeMid = 0.05f;
            emitter.sizeEnd = 0.02f;
            emitter.sizeMidPoint = 0.22f;
            emitter.spriteColumns = 4;
            emitter.spriteRows = 4;
            emitter.spriteFrameRate = 24.0f;
            emitter.randomStartFrame = true;
            emitter.spriteTexturePath = "assets/textures/particles/sparks_sheet.png";
            emitter.collideWithGround = true;
            emitter.collisionPlaneNormal = glm::vec3(0.0f, 1.0f, 0.0f);
            emitter.collisionPlaneOffset = 0.0f;
            emitter.bounceFactor = 0.42f;
            emitter.collisionFade = 0.55f;
            emitter.maxBounces = 2;
            return;
        case Preset::Magic:
            emitter.blendMode = SceneEntity::ParticleEmitterComponent::BlendMode::Premultiplied;
            emitter.maxParticles = 10000;
            emitter.emissionRate = 55.0f;
            emitter.particleLifetime = 2.8f;
            emitter.velocity = glm::vec3(0.0f, 0.65f, 0.0f);
            emitter.velocityVariance = glm::vec3(0.85f, 0.45f, 0.85f);
            emitter.positionVariance = glm::vec3(0.35f, 0.2f, 0.35f);
            emitter.acceleration = glm::vec3(0.0f, 0.15f, 0.0f);
            emitter.damping = 0.989f;
            emitter.looping = true;
            emitter.burstCount = 12;
            emitter.burstInterval = 0.75f;
            emitter.burstOnStart = true;
            emitter.colorStart = glm::vec4(0.35f, 0.85f, 1.0f, 0.95f);
            emitter.colorMid = glm::vec4(0.72f, 0.45f, 1.0f, 0.70f);
            emitter.colorEnd = glm::vec4(0.55f, 0.2f, 1.0f, 0.0f);
            emitter.colorMidPoint = 0.40f;
            emitter.sizeStart = 0.12f;
            emitter.sizeMid = 0.09f;
            emitter.sizeEnd = 0.04f;
            emitter.sizeMidPoint = 0.35f;
            emitter.spriteColumns = 4;
            emitter.spriteRows = 4;
            emitter.spriteFrameRate = 14.0f;
            emitter.randomStartFrame = true;
            emitter.spriteTexturePath = "assets/textures/particles/magic_sheet.png";
            emitter.collideWithGround = false;
            emitter.collisionPlaneNormal = glm::vec3(0.0f, 1.0f, 0.0f);
            emitter.collisionPlaneOffset = 0.0f;
            emitter.bounceFactor = 0.0f;
            emitter.collisionFade = 0.0f;
            emitter.maxBounces = 0;
            return;
    }
}

json vec3ToJson(const glm::vec3& v) {
    return json::array({v.x, v.y, v.z});
}

json vec4ToJson(const glm::vec4& v) {
    return json::array({v.x, v.y, v.z, v.w});
}

bool jsonToVec3(const json& value, glm::vec3& out) {
    if (!value.is_array() || value.size() != 3) {
        return false;
    }
    out.x = value[0].get<float>();
    out.y = value[1].get<float>();
    out.z = value[2].get<float>();
    return true;
}

bool jsonToVec4(const json& value, glm::vec4& out) {
    if (!value.is_array() || value.size() != 4) {
        return false;
    }
    out.x = value[0].get<float>();
    out.y = value[1].get<float>();
    out.z = value[2].get<float>();
    out.w = value[3].get<float>();
    return true;
}

bool nearlyEqual(float a, float b) {
    return std::abs(a - b) < 0.0001f;
}

glm::vec4 proceduralTintForTile(const ProceduralTileGenerator& generator, const GeneratedTile& tile) {
    const auto& cfg = generator.config();
    if (cfg.hasType(tile.type)) {
        return cfg.properties(tile.type).baseTint;
    }
    return tile.tint;
}

template <typename T>
bool vectorsEqual(const std::vector<T>& lhs, const std::vector<T>& rhs) {
    if (lhs.size() != rhs.size()) {
        return false;
    }
    for (std::size_t i = 0; i < lhs.size(); ++i) {
        if (!(lhs[i] == rhs[i])) {
            return false;
        }
    }
    return true;
}

} // namespace

Application::~Application() {
    shutdown();
}

bool Application::initialize(int argc, char** argv) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_EVENTS) != 0) {
        spdlog::error("SDL_Init failed: {}", SDL_GetError());
        return false;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 5);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

    m_window = SDL_CreateWindow(
        "Elysium Engine",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        m_windowWidth,
        m_windowHeight,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI
    );

    if (!m_window) {
        spdlog::error("SDL_CreateWindow failed: {}", SDL_GetError());
        return false;
    }

    m_glContext = SDL_GL_CreateContext(m_window);
    if (!m_glContext) {
        spdlog::error("SDL_GL_CreateContext failed: {}", SDL_GetError());
        return false;
    }

    if (SDL_GL_MakeCurrent(m_window, m_glContext) != 0) {
        spdlog::error("SDL_GL_MakeCurrent failed: {}", SDL_GetError());
        return false;
    }

    if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(SDL_GL_GetProcAddress))) {
        spdlog::error("Failed to initialize OpenGL loader (glad).");
        return false;
    }

    SDL_GL_SetSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

    applyElysiumDarkTheme();

    if (!ImGui_ImplSDL2_InitForOpenGL(m_window, m_glContext)) {
        spdlog::error("ImGui_ImplSDL2_InitForOpenGL failed.");
        return false;
    }

    if (!ImGui_ImplOpenGL3_Init("#version 450")) {
        spdlog::error("ImGui_ImplOpenGL3_Init failed.");
        return false;
    }

    // Rebuild fonts with configured size (default 26px = 2× ImGui's 13px)
    rebuildFonts();

    {
        const std::array<std::filesystem::path, 6> splashCandidates = {
            std::filesystem::current_path() / "image/elysium-logo.png",
            std::filesystem::current_path() / "image/elysium-logo.jpg",
            std::filesystem::current_path() / "../image/elysium-logo.png",
            std::filesystem::current_path() / "../image/elysium-logo.jpg",
            std::filesystem::current_path() / "../../image/elysium-logo.png",
            std::filesystem::current_path() / "../../image/elysium-logo.jpg"
        };

        for (const auto& candidate : splashCandidates) {
            if (std::filesystem::exists(candidate) && loadSplashTexture(candidate)) {
                appendLog("Loaded startup splash: " + candidate.lexically_normal().string());
                break;
            }
        }
    }

    m_running = true;
    if (m_splashTexture != 0 && !showStartupSplash(std::chrono::milliseconds(1200))) {
        return false;
    }

    if (!m_viewport.initialize()) {
        spdlog::error("Failed to initialize editor viewport.");
        return false;
    }

    {
        std::filesystem::path desiredRoot = std::filesystem::current_path();
        const std::filesystem::path projectAssets = std::filesystem::current_path() / "assets";
        if (std::filesystem::exists(projectAssets) && std::filesystem::is_directory(projectAssets)) {
            desiredRoot = projectAssets;
        }

        std::string error;
        if (!m_assetCatalog.setRoot(desiredRoot, &error)) {
            appendLog("Asset catalog init error: " + error);
        }
    }

    {
        auto scriptSubsystem = std::make_unique<ScriptRuntimeSubsystem>();
        scriptSubsystem->setContext(&m_scriptSystem, &m_scene, m_assetCatalog.root());
        m_scriptRuntimeSubsystem = scriptSubsystem.get();
        if (!m_subsystems.registerSubsystem(std::move(scriptSubsystem))) {
            appendLog("Failed to register ScriptRuntimeSubsystem.");
            return false;
        }

        auto physicsSubsystem = std::make_unique<PhysicsRuntimeSubsystem>();
        physicsSubsystem->setContext(&m_physicsSystem, &m_scene, &m_tileMap);
        m_physicsRuntimeSubsystem = physicsSubsystem.get();
        if (!m_subsystems.registerSubsystem(std::move(physicsSubsystem))) {
            appendLog("Failed to register PhysicsRuntimeSubsystem.");
            return false;
        }

        auto audioSubsystem = std::make_unique<SpatialAudioSystem>();
        m_spatialAudioSubsystem = audioSubsystem.get();
        if (!m_subsystems.registerSubsystem(std::move(audioSubsystem))) {
            appendLog("Failed to register SpatialAudioSystem.");
            return false;
        }

        auto networkingSubsystem = std::make_unique<NetworkingSystem>();
        m_networkingSubsystem = networkingSubsystem.get();
        if (!m_subsystems.registerSubsystem(std::move(networkingSubsystem))) {
            appendLog("Failed to register NetworkingSystem.");
            return false;
        }

        if (!m_subsystems.initializeAll([this](const std::string& line) {
                appendLog(line);
            })) {
            appendLog("SubsystemManager initialization failed.");
            return false;
        }
    }

    m_assetRootInput.fill('\0');
    {
        const std::string rootString = m_assetCatalog.root().string();
        std::strncpy(m_assetRootInput.data(), rootString.c_str(), m_assetRootInput.size() - 1U);
    }

    m_assetFilterInput.fill('\0');
    m_assetCatalog.setFilter("");
    m_luaConsoleInput.fill('\0');

    m_areaPathInput.fill('\0');
    {
        const std::string areaPathString = m_currentAreaPath.string();
        std::strncpy(m_areaPathInput.data(), areaPathString.c_str(), m_areaPathInput.size() - 1U);
    }

    appendLog("Elysium initialized.");
    loadStartupModelIfAny(argc, argv);

    clearHistory();
    m_undoStack.push_back(makeSnapshot());

    return true;
}

void Application::run() {
    using clock = std::chrono::high_resolution_clock;
    auto lastTick = clock::now();

    while (m_running) {
        auto now = clock::now();
        float dt = std::chrono::duration<float>(now - lastTick).count();
        lastTick = now;

        // Smooth FPS tracking (exponential moving average)
        constexpr float kFpsAlpha = 0.05f;
        if (dt > 0.0f) m_fpsSmoothed = m_fpsSmoothed * (1.0f - kFpsAlpha) + (1.0f / dt) * kFpsAlpha;
        m_frameTimeMs = dt * 1000.0f;

        pollEvents();
        if (m_fontsDirty) {
            m_fontsDirty = false;
            rebuildFonts();
        }
        syncSpatialAudio();
        m_subsystems.preUpdateAll(dt);
        m_subsystems.updateAll(dt);
        m_subsystems.postUpdateAll(dt);
        syncParticleEmitters();
        ParticleSystem::instance().update(dt);
        beginImGuiFrame();
        drawMainDockspace();
        drawViewportPanel(dt);
        drawAssetsPanel();
        drawOutlinerPanel();
        drawPropertiesPanel();
        drawConsolePanel();
        drawSettingsWindow();
        endImGuiFrame();
    }
}

void Application::shutdown() {
    for (auto& [entityId, emitter] : m_particleEmitters) {
        (void)entityId;
        if (emitter) {
            ParticleSystem::instance().destroyEmitter(emitter);
        }
    }
    m_particleEmitters.clear();

    m_subsystems.shutdownAll();
    m_scene.clear();
    m_viewport.shutdown();
    clearAssetThumbnailCache();
    destroySplashTexture();

    if (ImGui::GetCurrentContext()) {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplSDL2_Shutdown();
        ImGui::DestroyContext();
    }

    if (m_glContext) {
        SDL_GL_DeleteContext(m_glContext);
        m_glContext = nullptr;
    }

    if (m_window) {
        SDL_DestroyWindow(m_window);
        m_window = nullptr;
    }

    SDL_Quit();
}

void Application::clearAssetThumbnailCache() {
    for (auto& [path, texture] : m_assetThumbnailCache) {
        (void)path;
        if (texture != 0U) {
            glDeleteTextures(1, &texture);
        }
    }
    m_assetThumbnailCache.clear();
    m_failedAssetThumbnails.clear();
}

void Application::pollEvents() {
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
        ImGui_ImplSDL2_ProcessEvent(&ev);
        if (ev.type == SDL_QUIT) {
            m_running = false;
        }
        if (ev.type == SDL_WINDOWEVENT && ev.window.event == SDL_WINDOWEVENT_CLOSE) {
            m_running = false;
        }
        if (ev.type == SDL_DROPFILE && ev.drop.file != nullptr) {
            handleDroppedPath(std::filesystem::path{ev.drop.file});
            SDL_free(ev.drop.file);
        }
    }
}

void Application::beginImGuiFrame() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();
}

void Application::drawMainDockspace() {
    ImGuiWindowFlags windowFlags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);
    ImGui::SetNextWindowViewport(viewport->ID);

    windowFlags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse;
    windowFlags |= ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
    windowFlags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
    windowFlags |= ImGuiWindowFlags_NoBackground;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

    ImGui::Begin("ElysiumDockspaceHost", nullptr, windowFlags);
    ImGui::PopStyleVar(2);

    ImGuiID dockspaceID = ImGui::GetID("ElysiumDockspace");
    ImGui::DockSpace(dockspaceID, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);

    if (!m_dockLayoutInitialized || m_rebuildDockLayoutRequested) {
        applyDockPreset(dockspaceID, m_activeDockPreset);
        m_dockLayoutInitialized = true;
        m_rebuildDockLayoutRequested = false;
    }

    ImGuiIO& io = ImGui::GetIO();
    if (io.KeyCtrl && !io.WantTextInput) {
        if (ImGui::IsKeyPressed(ImGuiKey_G)) {
            if (io.KeyShift) {
                applyProceduralPreviewToTileMap();
            } else if (io.KeyAlt) {
                clearLastAppliedProceduralArea();
            } else {
                regenerateProceduralPreview();
            }
        }
        if (ImGui::IsKeyPressed(ImGuiKey_N)) {
            newArea();
        }
        if (ImGui::IsKeyPressed(ImGuiKey_O)) {
            loadAreaFromFile(m_currentAreaPath);
        }
        if (ImGui::IsKeyPressed(ImGuiKey_S)) {
            saveAreaToFile(m_currentAreaPath);
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Z)) {
            undo();
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Y)) {
            redo();
        }
    }

    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            ImGui::TextUnformatted("Area Path");
            ImGui::SetNextItemWidth(380.0f);
            if (ImGui::InputText("##AreaPath", m_areaPathInput.data(), m_areaPathInput.size())) {
                m_currentAreaPath = std::filesystem::path{m_areaPathInput.data()};
            }

            if (ImGui::MenuItem("New Area", "Ctrl+N")) {
                newArea();
            }
            if (ImGui::MenuItem("Load", "Ctrl+O")) {
                loadAreaFromFile(m_currentAreaPath);
            }
            if (ImGui::MenuItem("Save", "Ctrl+S")) {
                saveAreaToFile(m_currentAreaPath);
            }

            ImGui::Separator();
            if (ImGui::MenuItem("Quit", "Alt+F4")) {
                m_running = false;
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Settings")) {
            ImGui::MenuItem("Preferences...", nullptr, &m_showSettingsWindow);
            ImGui::EndMenu();
        }

        ImGui::Separator();
        ImGui::TextUnformatted("Gizmo");
        ImGui::SameLine();
        if (ImGui::SmallButton("W")) {
            m_gizmoMode = GizmoMode::Translate;
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("E")) {
            m_gizmoMode = GizmoMode::Rotate;
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("R")) {
            m_gizmoMode = GizmoMode::Scale;
        }

        ImGui::Separator();
        ImGui::Checkbox("Grid", &m_gridEnabled);
        ImGui::SameLine();
        ImGui::Checkbox("Brush", &m_tileBrushEnabled);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(90.0f);
        if (ImGui::DragFloat("Snap", &m_gridSnapSize, 0.05f, 0.1f, 100.0f, "%.2f")) {
            m_tileMap.setTileSize(m_gridSnapSize);
        }

        ImGui::Separator();
        if (ImGui::SmallButton("Generate Preview")) {
            regenerateProceduralPreview();
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Regenerate")) {
            ++m_proceduralParams.seed;
            regenerateProceduralPreview();
        }
        ImGui::SameLine();
        const bool hasPreview = m_proceduralPreviewArea.has_value() && !m_proceduralPreviewArea->tiles.empty();
        const bool hasAppliedProcedural = !m_lastAppliedProceduralRestore.empty();
        if (!hasPreview) {
            ImGui::BeginDisabled();
        }
        if (ImGui::SmallButton("Apply")) {
            applyProceduralPreviewToTileMap();
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Clear")) {
            clearProceduralPreview();
        }
        if (!hasPreview) {
            ImGui::EndDisabled();
        }
        ImGui::SameLine();
        if (!hasAppliedProcedural) {
            ImGui::BeginDisabled();
        }
        if (ImGui::SmallButton("Clear Applied")) {
            clearLastAppliedProceduralArea();
        }
        if (!hasAppliedProcedural) {
            ImGui::EndDisabled();
        }

        ImGui::Separator();

        // Walkmesh preview toggle ([P] shortcut handled in handleViewportShortcuts).
        ImGui::PushStyleColor(ImGuiCol_Button,
            m_walkmeshPreviewEnabled ? ImVec4(0.18f, 0.68f, 0.30f, 1.0f)
                                     : ImVec4(0.25f, 0.27f, 0.30f, 1.0f));
        if (ImGui::SmallButton("Preview [P]")) {
            m_walkmeshPreviewEnabled = !m_walkmeshPreviewEnabled;
            if (m_walkmeshPreviewEnabled) {
                rebuildWalkmesh();
                // Place character at Y = walkmesh surface at origin.
                glm::vec3 startPos{0.0f};
                const auto h = m_tileWalkmesh.sampleHeight(0.0f, 0.0f);
                if (h.has_value()) startPos.y = h.value();
                m_controller.reset(startPos);
            }
        }
        ImGui::PopStyleColor();
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Button,
            m_walkmeshDebugEnabled ? ImVec4(0.18f, 0.45f, 0.72f, 1.0f)
                                   : ImVec4(0.25f, 0.27f, 0.30f, 1.0f));
        if (ImGui::SmallButton("Walkmesh")) {
            m_walkmeshDebugEnabled = !m_walkmeshDebugEnabled;
            m_viewport.setWalkmeshDebugEnabled(m_walkmeshDebugEnabled);
            if (m_walkmeshDebugEnabled) {
                rebuildWalkmesh();
                m_viewport.uploadWalkmeshLines(
                    m_tileWalkmesh.buildWalkableLines(),
                    m_tileWalkmesh.buildBlockedLines());
            }
        }
        ImGui::PopStyleColor();

        ImGui::Separator();
        bool physicsRunning = m_physicsSystem.simulationEnabled();
        if (ImGui::Checkbox("Physics", &physicsRunning)) {
            m_physicsSystem.setSimulationEnabled(physicsRunning);
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Step Physics")) {
            m_physicsSystem.requestSingleStep();
        }
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Button,
            m_physicsDebugEnabled ? ImVec4(0.62f, 0.54f, 0.22f, 1.0f)
                                 : ImVec4(0.25f, 0.27f, 0.30f, 1.0f));
        if (ImGui::SmallButton("Physics Debug")) {
            m_physicsDebugEnabled = !m_physicsDebugEnabled;
            m_viewport.setPhysicsDebugEnabled(m_physicsDebugEnabled);
            if (m_physicsDebugEnabled) {
                m_viewport.uploadPhysicsLines(m_physicsSystem.buildDebugLines());
            }
        }
        ImGui::PopStyleColor();
        ImGui::SameLine();
        ImGui::Checkbox("Viewport Stats", &m_viewportDebugOverlayEnabled);
        ImGui::SameLine();
        ImGui::Text("Bodies: %zu", m_physicsSystem.bodyCount());

        ImGui::Separator();
        ImGui::Text("Layout: %s", dockPresetLabel(m_activeDockPreset));
        ImGui::SameLine();
        if (ImGui::SmallButton("Default")) {
            m_activeDockPreset = DockPreset::Default;
            m_rebuildDockLayoutRequested = true;
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Terrain")) {
            m_activeDockPreset = DockPreset::Terrain;
            m_rebuildDockLayoutRequested = true;
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Asset Prep")) {
            m_activeDockPreset = DockPreset::AssetPrep;
            m_rebuildDockLayoutRequested = true;
        }

        ImGui::Separator();
        ImGui::PushStyleColor(ImGuiCol_Button, m_viewport.drawSkyEnabled()
            ? ImVec4(0.20f, 0.40f, 0.70f, 1.0f)
            : ImVec4(0.25f, 0.27f, 0.30f, 1.0f));
        if (ImGui::SmallButton("Sky")) {
            m_viewport.setDrawSky(!m_viewport.drawSkyEnabled());
        }
        ImGui::PopStyleColor();
        ImGui::SameLine();
        ImGui::Text("%.0f FPS  %.1f ms", m_fpsSmoothed, m_frameTimeMs);

        ImGui::Separator();
        ImGui::TextUnformatted("Elysium Engine");
        ImGui::EndMenuBar();
    }

    ImGui::End();
}

void Application::applyDockPreset(ImGuiID dockspaceID, DockPreset preset) {
    ImGui::DockBuilderRemoveNode(dockspaceID);
    ImGui::DockBuilderAddNode(dockspaceID, ImGuiDockNodeFlags_None);
    ImGui::DockBuilderSetNodeSize(dockspaceID, ImGui::GetMainViewport()->Size);

    ImGuiID root = dockspaceID;
    ImGuiID left = 0;
    ImGuiID right = 0;
    ImGuiID bottom = 0;
    ImGuiID leftBottom = 0;

    float leftRatio = 0.22f;
    float rightRatio = 0.24f;
    float bottomRatio = 0.24f;
    float leftBottomRatio = 0.42f;

    if (preset == DockPreset::Terrain) {
        leftRatio = 0.18f;
        rightRatio = 0.22f;
        bottomRatio = 0.28f;
        leftBottomRatio = 0.35f;
    } else if (preset == DockPreset::AssetPrep) {
        leftRatio = 0.30f;
        rightRatio = 0.20f;
        bottomRatio = 0.20f;
        leftBottomRatio = 0.25f;
    }

    ImGui::DockBuilderSplitNode(root, ImGuiDir_Left, leftRatio, &left, &root);
    ImGui::DockBuilderSplitNode(root, ImGuiDir_Right, rightRatio, &right, &root);
    ImGui::DockBuilderSplitNode(root, ImGuiDir_Down, bottomRatio, &bottom, &root);
    ImGui::DockBuilderSplitNode(left, ImGuiDir_Down, leftBottomRatio, &leftBottom, &left);

    ImGui::DockBuilderDockWindow("Viewport", root);
    ImGui::DockBuilderDockWindow("Assets Browser", left);
    ImGui::DockBuilderDockWindow("Outliner", leftBottom);
    ImGui::DockBuilderDockWindow("Properties", right);
    ImGui::DockBuilderDockWindow("Console", bottom);

    ImGui::DockBuilderFinish(dockspaceID);
}

void Application::drawViewportPanel(float dt) {
    ImGui::Begin("Viewport");

    handleViewportShortcuts();

    // Lazy walkmesh CPU rebuild (GPU upload happens inside rebuildWalkmesh).
    if (m_walkmeshDirty) {
        rebuildWalkmesh();
    }

    ImGui::Checkbox("Snap New Placements", &m_snapNewPlacements);
    if (m_tileBrushEnabled) {
        ImGui::SameLine();
        ImGui::SetNextItemWidth(180.0f);
        ImGui::ColorEdit4("Tile Tint", &m_tileBrushTint.x);
        ImGui::SameLine();
        ImGui::Checkbox("Walkable##brush", &m_brushWalkable);
        ImGui::SameLine();
        ImGui::TextDisabled("LMB: Place  Shift+LMB: Erase");
    }

    if (ImGui::CollapsingHeader("Procedural Area", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::SetNextItemWidth(180.0f);
        ImGui::InputScalar("Seed", ImGuiDataType_U32, &m_proceduralParams.seed);
        ImGui::SameLine();
        if (ImGui::SmallButton("+1 Seed")) {
            ++m_proceduralParams.seed;
            regenerateProceduralPreview();
        }

        ImGui::SetNextItemWidth(120.0f);
        ImGui::DragInt("Width", &m_proceduralParams.width, 1.0f, 1, 512);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(120.0f);
        ImGui::DragInt("Height", &m_proceduralParams.height, 1.0f, 1, 512);

        ImGui::SetNextItemWidth(120.0f);
        ImGui::DragInt("Origin X", &m_proceduralParams.originX, 1.0f, -4096, 4096);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(120.0f);
        ImGui::DragInt("Origin Z", &m_proceduralParams.originZ, 1.0f, -4096, 4096);

        ImGui::SetNextItemWidth(200.0f);
        ImGui::SliderFloat("Water Level", &m_proceduralParams.waterLevel, 0.0f, 0.95f, "%.2f");
        ImGui::SetNextItemWidth(200.0f);
        ImGui::SliderFloat("Beach Level", &m_proceduralParams.beachLevel, 0.0f, 0.98f, "%.2f");
        if (m_proceduralParams.beachLevel < m_proceduralParams.waterLevel) {
            m_proceduralParams.beachLevel = m_proceduralParams.waterLevel;
        }

        ImGui::SetNextItemWidth(200.0f);
        ImGui::SliderFloat("Transition", &m_proceduralParams.transitionIntensity, 0.0f, 2.0f, "%.2f");
        ImGui::SetNextItemWidth(200.0f);
        ImGui::SliderFloat("Edge Jitter", &m_proceduralParams.edgeJitterStrength, 0.0f, 2.0f, "%.2f");
        ImGui::Checkbox("Overlay Preview", &m_proceduralOverlayEnabled);

        if (ImGui::TreeNode("Noise")) {
            ImGui::SetNextItemWidth(200.0f);
            ImGui::SliderFloat("Height Freq", &m_proceduralParams.noise.heightFrequency, 0.002f, 0.2f, "%.3f");
            ImGui::SetNextItemWidth(200.0f);
            ImGui::SliderFloat("Moisture Freq", &m_proceduralParams.noise.moistureFrequency, 0.002f, 0.2f, "%.3f");
            ImGui::SetNextItemWidth(200.0f);
            ImGui::SliderFloat("Temperature Freq", &m_proceduralParams.noise.temperatureFrequency, 0.002f, 0.2f, "%.3f");
            ImGui::SetNextItemWidth(200.0f);
            ImGui::SliderFloat("Detail Freq", &m_proceduralParams.noise.detailFrequency, 0.01f, 0.6f, "%.3f");
            ImGui::SetNextItemWidth(200.0f);
            ImGui::SliderInt("Octaves", &m_proceduralParams.noise.octaves, 1, 8);
            ImGui::SetNextItemWidth(200.0f);
            ImGui::SliderFloat("Lacunarity", &m_proceduralParams.noise.lacunarity, 1.2f, 3.5f, "%.2f");
            ImGui::SetNextItemWidth(200.0f);
            ImGui::SliderFloat("Gain", &m_proceduralParams.noise.gain, 0.2f, 0.9f, "%.2f");
            ImGui::TreePop();
        }

        auto drawBiomeWeight = [&](TileType type, const char* label) {
            float weight = 0.0f;
            auto it = m_proceduralParams.biomeWeights.find(type);
            if (it != m_proceduralParams.biomeWeights.end()) {
                weight = it->second;
            }
            ImGui::SetNextItemWidth(160.0f);
            if (ImGui::SliderFloat(label, &weight, -1.0f, 1.0f, "%.2f")) {
                if (std::abs(weight) < 0.0001f) {
                    m_proceduralParams.biomeWeights.erase(type);
                } else {
                    m_proceduralParams.biomeWeights[type] = weight;
                }
            }
        };

        if (ImGui::TreeNode("Biome Weights")) {
            drawBiomeWeight(TileType::Grass, "Grass Bias");
            drawBiomeWeight(TileType::Sand, "Sand Bias");
            drawBiomeWeight(TileType::Rocky, "Rocky Bias");
            drawBiomeWeight(TileType::Snow, "Snow Bias");
            drawBiomeWeight(TileType::Swamp, "Swamp Bias");
            drawBiomeWeight(TileType::Cobblestone, "Cobble Bias");
            ImGui::TreePop();
        }

        if (ImGui::Button("Generate Preview##ProceduralPanel")) {
            regenerateProceduralPreview();
        }
        ImGui::SameLine();
        if (ImGui::Button("Regenerate##ProceduralPanel")) {
            ++m_proceduralParams.seed;
            regenerateProceduralPreview();
        }
        ImGui::SameLine();
        const bool hasPreview = m_proceduralPreviewArea.has_value() && !m_proceduralPreviewArea->tiles.empty();
        const bool hasAppliedProcedural = !m_lastAppliedProceduralRestore.empty();
        if (!hasPreview) {
            ImGui::BeginDisabled();
        }
        if (ImGui::Button("Apply##ProceduralPanel")) {
            applyProceduralPreviewToTileMap();
        }
        ImGui::SameLine();
        if (ImGui::Button("Clear##ProceduralPanel")) {
            clearProceduralPreview();
        }
        if (!hasPreview) {
            ImGui::EndDisabled();
        }

        ImGui::SameLine();
        if (!hasAppliedProcedural) {
            ImGui::BeginDisabled();
        }
        if (ImGui::Button("Clear Applied##ProceduralPanel")) {
            clearLastAppliedProceduralArea();
        }
        if (!hasAppliedProcedural) {
            ImGui::EndDisabled();
        }

        if (hasPreview) {
            const GeneratedArea& preview = *m_proceduralPreviewArea;
            ImGui::TextDisabled("Preview: %d x %d @ (%d, %d), %zu tiles",
                               preview.width,
                               preview.height,
                               preview.originX,
                               preview.originZ,
                               preview.tiles.size());
        } else {
            ImGui::TextDisabled("No preview generated.");
        }
        ImGui::TextDisabled("Shortcuts: Ctrl+G generate, Ctrl+Shift+G apply, Ctrl+Alt+G clear applied");
    }

    if (ImGui::CollapsingHeader("Render Debug", ImGuiTreeNodeFlags_DefaultOpen)) {
        const char* debugViews[] = {"Lit", "Normals", "Roughness", "Metallic", "Emissive"};
        int debugView = static_cast<int>(m_viewport.materialDebugView());
        if (ImGui::Combo("Material View", &debugView, debugViews, IM_ARRAYSIZE(debugViews))) {
            m_viewport.setMaterialDebugView(static_cast<EditorViewport::MaterialDebugView>(debugView));
        }

        bool toneMappingEnabled = m_viewport.toneMappingEnabled();
        if (ImGui::Checkbox("Tone Mapping", &toneMappingEnabled)) {
            m_viewport.setToneMappingEnabled(toneMappingEnabled);
        }

        float exposure = m_viewport.exposure();
        if (ImGui::SliderFloat("Exposure", &exposure, 0.25f, 3.0f, "%.2f")) {
            m_viewport.setExposure(exposure);
        }

        float gamma = m_viewport.gamma();
        if (ImGui::SliderFloat("Gamma", &gamma, 1.6f, 2.6f, "%.2f")) {
            m_viewport.setGamma(gamma);
        }
    }

    m_viewport.setGridEnabled(m_gridEnabled);
    m_tileMap.setTileSize(m_gridSnapSize);

    ImVec2 avail = ImGui::GetContentRegionAvail();
    if (avail.x > 1.0f && avail.y > 1.0f) {
        m_viewport.resize(static_cast<int>(avail.x), static_cast<int>(avail.y));
        m_viewport.updateCamera(dt, ImGui::IsWindowHovered(), ImGui::IsWindowFocused());
        if (m_physicsDebugEnabled) {
            m_viewport.uploadPhysicsLines(m_physicsSystem.buildDebugLines());
        }
        m_viewport.render(m_scene, m_tileMap);

        const ImTextureID texID = static_cast<ImTextureID>(static_cast<uintptr_t>(m_viewport.colorTexture()));
        ImGui::Image(texID, avail, ImVec2(0, 1), ImVec2(1, 0));

        const ImVec2 imageMin = ImGui::GetItemRectMin();
        const ImVec2 imageMax = ImGui::GetItemRectMax();

        if (m_viewportDebugOverlayEnabled) {
            ImDrawList* draw = ImGui::GetWindowDrawList();
            const ImVec2 p0(imageMin.x + 10.0f, imageMin.y + 10.0f);
            const ImVec2 p1(imageMin.x + 368.0f, imageMin.y + 104.0f);
            draw->AddRectFilled(p0, p1, IM_COL32(16, 19, 24, 190), 6.0f);
            draw->AddRect(p0, p1, IM_COL32(180, 190, 210, 120), 6.0f, 0, 1.0f);

            std::size_t lightCount = 0;
            for (const auto& entity : m_scene.entities()) {
                if (entity.light.has_value() && entity.light->enabled) {
                    ++lightCount;
                }
            }

            const char* debugView = materialDebugViewLabel(m_viewport.materialDebugView());
            char line0[128];
            char line1[160];
            char line2[160];
            char line3[160];
            std::snprintf(line0, sizeof(line0), "FPS %.1f | Frame %.2f ms", m_fpsSmoothed, m_frameTimeMs);
            const std::size_t audioEmitters = (m_spatialAudioSubsystem != nullptr) ? m_spatialAudioSubsystem->submittedEmitterCount() : 0U;
            std::snprintf(line1, sizeof(line1), "Entities %zu | Lights %zu | Bodies %zu | Audio %zu", m_scene.entities().size(), lightCount, m_physicsSystem.bodyCount(), audioEmitters);
            std::snprintf(line2, sizeof(line2), "View %s | Tonemap %s | Exp %.2f | Gamma %.2f", debugView, m_viewport.toneMappingEnabled() ? "On" : "Off", m_viewport.exposure(), m_viewport.gamma());
            std::snprintf(line3, sizeof(line3), "Walkmesh tris %zu | Physics %s", m_tileWalkmesh.triangleCount(), m_physicsSystem.simulationEnabled() ? "Running" : "Paused");

            draw->AddText(ImVec2(p0.x + 10.0f, p0.y + 8.0f), IM_COL32(230, 236, 246, 255), line0);
            draw->AddText(ImVec2(p0.x + 10.0f, p0.y + 30.0f), IM_COL32(210, 219, 234, 255), line1);
            draw->AddText(ImVec2(p0.x + 10.0f, p0.y + 52.0f), IM_COL32(210, 219, 234, 255), line2);
            draw->AddText(ImVec2(p0.x + 10.0f, p0.y + 74.0f), IM_COL32(210, 219, 234, 255), line3);
        }

        handleViewportSelectionAndGizmo(imageMin, imageMax);
        drawViewportGizmoOverlay(imageMin, imageMax);
        drawProceduralPreviewOverlay(imageMin, imageMax);

        // Character preview overlay — update movement then draw a 2-D avatar.
        if (m_walkmeshPreviewEnabled) {
            const bool viewportActive = ImGui::IsWindowHovered() && ImGui::IsWindowFocused();
            m_controller.update(dt, m_tileWalkmesh, m_viewport.cameraYawDeg(), viewportActive);
            drawCharacterPreview(imageMin, imageMax);
        }

        if (m_tileBrushEnabled && ImGui::IsItemHovered()) {
            ImGuiIO& io = ImGui::GetIO();
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !io.KeyAlt) {
                glm::vec3 hitPoint{0.0f};
                if (m_viewport.screenPointToGround(io.MousePos, imageMin, imageMax, hitPoint)) {
                    int cellX = 0;
                    int cellZ = 0;
                    if (m_tileMap.worldToCell(hitPoint, cellX, cellZ)) {
                        if (io.KeyShift) {
                            captureUndoSnapshot("Erase tile");
                            if (m_tileMap.eraseTile(cellX, cellZ)) {
                                m_walkmeshDirty = true;
                                appendLog("Erased tile at (" + std::to_string(cellX) + ", " + std::to_string(cellZ) + ")");
                            }
                        } else {
                            captureUndoSnapshot("Paint tile");
                            TileData tile{};
                            tile.tint     = m_tileBrushTint;
                            tile.walkable = m_brushWalkable;
                            const bool created = m_tileMap.placeTile(cellX, cellZ, tile);
                            m_walkmeshDirty = true;
                            appendLog(std::string(created ? "Placed" : "Updated") + " tile at (" + std::to_string(cellX) + ", " + std::to_string(cellZ) + ")");
                        }
                    }
                }
            }
        }

        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ELYSIUM_ASSET_PATH")) {
                if (payload->Data != nullptr && payload->DataSize > 0) {
                    const auto* dropped = static_cast<const char*>(payload->Data);
                    instantiateModel(std::filesystem::path{dropped});
                }
            }
            ImGui::EndDragDropTarget();
        }
    }
    
    ImGui::End();
}

void Application::drawAssetsPanel() {
    ImGui::Begin("Assets Browser");
    ImGui::TextUnformatted("Root Directory");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::InputText("##AssetRoot", m_assetRootInput.data(), m_assetRootInput.size());

    if (ImGui::Button("Set Root")) {
        std::string error;
        if (m_assetCatalog.setRoot(std::filesystem::path{m_assetRootInput.data()}, &error)) {
            clearAssetThumbnailCache();
            m_scriptSystem.setScriptRoot(m_assetCatalog.root());
            appendLog("Asset root set: " + m_assetCatalog.root().string());
        } else {
            appendLog("Asset root error: " + error);
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Use CWD")) {
        std::string error;
        const auto cwd = std::filesystem::current_path();
        if (m_assetCatalog.setRoot(cwd, &error)) {
            clearAssetThumbnailCache();
            const std::string rootString = m_assetCatalog.root().string();
            m_assetRootInput.fill('\0');
            std::strncpy(m_assetRootInput.data(), rootString.c_str(), m_assetRootInput.size() - 1U);
            m_scriptSystem.setScriptRoot(m_assetCatalog.root());
            appendLog("Asset root set to CWD: " + rootString);
        } else {
            appendLog("Asset root error: " + error);
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Rescan")) {
        std::string error;
        if (m_assetCatalog.rescan(&error)) {
            clearAssetThumbnailCache();
            appendLog("Asset rescan complete.");
        } else {
            appendLog("Asset rescan error: " + error);
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Refresh Previews")) {
        clearAssetThumbnailCache();
        appendLog("Asset thumbnail cache cleared.");
    }

    ImGui::Separator();
    if (ImGui::InputTextWithHint("##AssetFilter", "Filter assets...", m_assetFilterInput.data(), m_assetFilterInput.size())) {
        m_assetCatalog.setFilter(m_assetFilterInput.data());
    }
    bool showAuthoringSourceFormats = m_assetCatalog.showAuthoringSourceFormats();
    if (ImGui::Checkbox("Show Source Formats (FBX)", &showAuthoringSourceFormats)) {
        m_assetCatalog.setShowAuthoringSourceFormats(showAuthoringSourceFormats);
        clearAssetThumbnailCache();
        appendLog(showAuthoringSourceFormats
            ? "Authoring/source formats are now visible in Assets Browser."
            : "Authoring/source formats are hidden (glTF-first view)."
        );
    }

    const auto& allEntries = m_assetCatalog.allEntries();
    const auto& entries = m_assetCatalog.filteredEntries();
    ImGui::Text("Assets: %zu shown / %zu indexed", entries.size(), allEntries.size());
    ImGui::Text("Tip: group packs under folders like characters/environment/props/tiles.");
    ImGui::Separator();

    std::map<AssetType, std::map<std::string, std::vector<const AssetEntry*>>> grouped;
    for (const auto& entry : entries) {
        grouped[entry.type][assetCategoryFromRelativePath(entry.relativePath)].push_back(&entry);
    }

    static std::string selectedAssetPath;
    const float tileWidth = 108.0f;
    const float tileHeight = 112.0f;

    for (auto& [type, byCategory] : grouped) {
        const std::string typeHeader = std::string(assetTypeLabel(type)) + "##assetType" + std::to_string(static_cast<int>(type));
        if (!ImGui::CollapsingHeader(typeHeader.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
            continue;
        }

        for (auto& [category, categoryEntries] : byCategory) {
            const std::string categoryHeader = category + "##assetCategory" + std::to_string(static_cast<int>(type));
            if (!ImGui::TreeNodeEx(categoryHeader.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
                continue;
            }

            const float contentWidth = ImGui::GetContentRegionAvail().x;
            int columns = static_cast<int>(contentWidth / (tileWidth + 8.0f));
            columns = std::max(1, columns);

            for (std::size_t i = 0; i < categoryEntries.size(); ++i) {
                const AssetEntry* entry = categoryEntries[i];
                if (entry == nullptr) {
                    continue;
                }

                ImGui::PushID(entry->relativePath.c_str());
                if (i > 0 && (static_cast<int>(i) % columns) != 0) {
                    ImGui::SameLine();
                }

                const ImVec2 cursor = ImGui::GetCursorScreenPos();
                const bool selected = selectedAssetPath == entry->absolutePath.string();
                const ImU32 borderColor = selected ? IM_COL32(255, 255, 255, 220) : IM_COL32(88, 92, 99, 190);
                const ImU32 cardColor = selected ? IM_COL32(40, 46, 56, 220) : IM_COL32(30, 34, 40, 210);

                ImGui::InvisibleButton("##assetTile", ImVec2(tileWidth, tileHeight));
                const bool hovered = ImGui::IsItemHovered();
                const bool pressed = ImGui::IsItemClicked(ImGuiMouseButton_Left);
                if (pressed) {
                    selectedAssetPath = entry->absolutePath.string();
                    if (entry->type == AssetType::Model) {
                        instantiateModel(entry->absolutePath);
                    } else {
                        appendLog("Selected asset: " + entry->relativePath + " (drag onto a compatible field to assign)");
                    }
                }

                ImDrawList* draw = ImGui::GetWindowDrawList();
                draw->AddRectFilled(cursor, ImVec2(cursor.x + tileWidth, cursor.y + tileHeight), cardColor, 8.0f);
                draw->AddRect(cursor, ImVec2(cursor.x + tileWidth, cursor.y + tileHeight), hovered ? IM_COL32(220, 220, 220, 220) : borderColor, 8.0f, 0, hovered ? 2.0f : 1.0f);

                const ImVec2 iconMin(cursor.x + 10.0f, cursor.y + 10.0f);
                const ImVec2 iconMax(cursor.x + tileWidth - 10.0f, cursor.y + 62.0f);
                bool drewTexturePreview = false;
                if (entry->type == AssetType::Texture || entry->type == AssetType::Model) {
                    const std::string thumbKey = entry->absolutePath.lexically_normal().string();
                    auto thumbIt = m_assetThumbnailCache.find(thumbKey);
                    if (thumbIt == m_assetThumbnailCache.end() && m_failedAssetThumbnails.find(thumbKey) == m_failedAssetThumbnails.end()) {
                        GLuint texture = 0;
                        bool created = false;
                        if (entry->type == AssetType::Texture) {
                            created = createTextureThumbnailFromFile(entry->absolutePath, texture);
                        } else {
                            created = createModelThumbnail(*entry, m_loader, m_modelCache, hovered || selected, texture);
                        }
                        if (created) {
                            thumbIt = m_assetThumbnailCache.emplace(thumbKey, texture).first;
                        } else {
                            m_failedAssetThumbnails.insert(thumbKey);
                        }
                    }

                    if (thumbIt != m_assetThumbnailCache.end() && thumbIt->second != 0U) {
                        draw->AddImage(
                            static_cast<ImTextureID>(static_cast<uintptr_t>(thumbIt->second)),
                            iconMin,
                            iconMax,
                            ImVec2(0.0f, 1.0f),
                            ImVec2(1.0f, 0.0f));
                        draw->AddRect(iconMin, iconMax, IM_COL32(220, 220, 220, 110), 6.0f, 0, 1.0f);
                        drewTexturePreview = true;
                    }
                }

                if (!drewTexturePreview) {
                    draw->AddRectFilled(iconMin, iconMax, assetTypeColor(entry->type), 6.0f);
                    const char* badge = assetTypeBadge(entry->type);
                    const ImVec2 badgeSize = ImGui::CalcTextSize(badge);
                    draw->AddText(ImVec2(iconMin.x + (iconMax.x - iconMin.x - badgeSize.x) * 0.5f,
                                         iconMin.y + (iconMax.y - iconMin.y - badgeSize.y) * 0.5f),
                                  IM_COL32(25, 25, 25, 255),
                                  badge);
                }

                const std::string displayName = std::filesystem::path(entry->relativePath).stem().string();
                const char* label = displayName.empty() ? entry->relativePath.c_str() : displayName.c_str();
                const ImVec2 textPos(cursor.x + 10.0f, cursor.y + 72.0f);
                draw->AddText(textPos, IM_COL32(230, 232, 235, 245), label);

                if (hovered) {
                    ImGui::SetTooltip("%s", entry->relativePath.c_str());
                }

                if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
                    const std::string fullPath = entry->absolutePath.string();
                    ImGui::SetDragDropPayload("ELYSIUM_ASSET_PATH", fullPath.c_str(), fullPath.size() + 1U);
                    ImGui::TextUnformatted(entry->relativePath.c_str());
                    ImGui::EndDragDropSource();
                }

                ImGui::PopID();
            }

            ImGui::TreePop();
        }
    }

    ImGui::End();
}

void Application::drawOutlinerPanel() {
    ImGui::Begin("Outliner");
    const auto& entities = m_scene.entities();
    for (const auto& entity : entities) {
        const bool selected = m_selectedEntityId.has_value() && m_selectedEntityId.value() == entity.id;
        if (ImGui::Selectable(entity.name.c_str(), selected)) {
            m_selectedEntityId = entity.id;
        }
    }

    if (ImGui::IsKeyPressed(ImGuiKey_Delete) && m_selectedEntityId.has_value()) {
        captureUndoSnapshot("Delete entity");
        const bool removed = m_scene.destroyEntity(m_selectedEntityId.value());
        m_selectedEntityId.reset();
        appendLog(removed ? "Deleted selected entity." : "Selected entity no longer exists.");
    }

    ImGui::End();
}

void Application::drawPropertiesPanel() {
    ImGui::Begin("Properties");

    SceneEntity* selectedEntity = nullptr;
    if (m_selectedEntityId.has_value()) {
        selectedEntity = m_scene.findEntity(m_selectedEntityId.value());
    }

    if (selectedEntity != nullptr) {
        const bool propertyInteractionStarted = ImGui::IsMouseClicked(ImGuiMouseButton_Left) && ImGui::IsWindowHovered();
        if (propertyInteractionStarted && !m_propertyEditActive) {
            captureUndoSnapshot("Edit entity properties");
            m_propertyEditActive = true;
        }

        ImGui::Text("Entity ID: %llu", static_cast<unsigned long long>(selectedEntity->id));
        ImGui::Text("Mode: %s", gizmoModeLabel(m_gizmoMode));
        ImGui::InputText("Name", selectedEntity->nameBuffer.data(), selectedEntity->nameBuffer.size());
        selectedEntity->name = selectedEntity->nameBuffer.data();

        ImGui::DragFloat3("Position", &selectedEntity->transform.position.x, 0.05f);
        if (ImGui::Button("Snap Position To Grid")) {
            captureUndoSnapshot("Snap entity to grid");
            snapEntityToGrid(*selectedEntity);
        }
        ImGui::DragFloat3("Rotation (deg)", &selectedEntity->transform.rotationEulerDeg.x, 0.5f);
        ImGui::DragFloat3("Scale", &selectedEntity->transform.scale.x, 0.01f, 0.01f, 100.0f);
        ImGui::ColorEdit4("Tint", &selectedEntity->tint.x);

        ImGui::Separator();
        ImGui::TextUnformatted("Script");
        if (!selectedEntity->script.has_value()) {
            selectedEntity->script = SceneEntity::ScriptComponent{};
        }

        ImGui::Checkbox("Enabled##Script", &selectedEntity->script->enabled);

        std::array<char, 512> scriptPathBuffer{};
        std::strncpy(scriptPathBuffer.data(), selectedEntity->script->scriptPath.c_str(), scriptPathBuffer.size() - 1U);
        if (ImGui::InputText("Script Path", scriptPathBuffer.data(), scriptPathBuffer.size())) {
            selectedEntity->script->scriptPath = scriptPathBuffer.data();
        }

        if (ImGui::Button("Run on_interact##Script")) {
            if (!m_scriptSystem.invokeInteract(*selectedEntity)) {
                appendLog("Script on_interact not available for selected entity.");
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Run on_enter_trigger##Script")) {
            if (!m_scriptSystem.invokeEnterTrigger(*selectedEntity, "editor_test_trigger")) {
                appendLog("Script on_enter_trigger not available for selected entity.");
            }
        }

        ImGui::Separator();
        ImGui::TextUnformatted("Physics");
        if (!selectedEntity->physics.has_value()) {
            selectedEntity->physics = SceneEntity::PhysicsComponent{};
        }
        ImGui::Checkbox("Enabled##Physics", &selectedEntity->physics->enabled);
        ImGui::Checkbox("Dynamic##Physics", &selectedEntity->physics->dynamic);
        ImGui::DragFloat3("Half Extents##Physics", &selectedEntity->physics->halfExtents.x, 0.01f, 0.05f, 20.0f, "%.2f");

        ImGui::Separator();
        ImGui::TextUnformatted("Light");
        if (!selectedEntity->light.has_value()) {
            if (ImGui::Button("Add Light Component")) {
                selectedEntity->light = SceneEntity::LightComponent{};
            }
        } else {
            if (ImGui::Button("Remove Light Component")) {
                selectedEntity->light.reset();
            } else {
                auto& lc = *selectedEntity->light;
                ImGui::Checkbox("Enabled##LightComponent", &lc.enabled);
                int lightType = static_cast<int>(lc.type);
                if (ImGui::Combo("Type##Light", &lightType, "Point\0Directional\0")) {
                    lc.type = static_cast<SceneEntity::LightComponent::Type>(lightType);
                }
                ImGui::ColorEdit3("Color##Light", &lc.color.x);
                ImGui::DragFloat("Intensity##Light", &lc.intensity, 0.01f, 0.0f, 100.0f, "%.2f");
                if (lc.type == SceneEntity::LightComponent::Type::Point) {
                    ImGui::DragFloat("Range##Light", &lc.range, 0.1f, 0.1f, 500.0f, "%.1f");
                }
            }
        }

        ImGui::Separator();
        ImGui::TextUnformatted("Particle Emitter");
        if (!selectedEntity->particleEmitter.has_value()) {
            if (ImGui::Button("Add Particle Emitter")) {
                selectedEntity->particleEmitter = SceneEntity::ParticleEmitterComponent{};
            }
        } else {
            if (ImGui::Button("Remove Particle Emitter")) {
                selectedEntity->particleEmitter.reset();
            } else {
                auto& pe = *selectedEntity->particleEmitter;
                bool markCustom = false;
                ImGui::Checkbox("Enabled##ParticleEmitter", &pe.enabled);
                ImGui::TextDisabled("Preset: %s", particlePresetLabel(pe.preset));
                int preset = static_cast<int>(pe.preset);
                if (ImGui::Combo("Preset##PE", &preset, "Custom\0Fire\0Smoke\0Sparks\0Magic\0")) {
                    pe.preset = static_cast<SceneEntity::ParticleEmitterComponent::Preset>(preset);
                    applyParticleEmitterPreset(pe);
                }
                ImGui::SameLine();
                if (ImGui::SmallButton("Apply Preset##PE") && pe.preset != SceneEntity::ParticleEmitterComponent::Preset::Custom) {
                    applyParticleEmitterPreset(pe);
                }
                int blendMode = static_cast<int>(pe.blendMode);
                if (ImGui::Combo("Blend Mode##PE", &blendMode, "Alpha\0Additive\0Premultiplied\0")) {
                    pe.blendMode = static_cast<SceneEntity::ParticleEmitterComponent::BlendMode>(blendMode);
                    markCustom = true;
                }
                ImGui::TextDisabled("Blend: %s", particleBlendModeLabel(pe.blendMode));
                markCustom |= ImGui::DragScalar("Max Particles##PE", ImGuiDataType_U32, &pe.maxParticles, 100.0f, nullptr, nullptr, "%u");
                markCustom |= ImGui::DragFloat("Emission Rate##PE", &pe.emissionRate, 1.0f, 0.0f, 10000.0f, "%.0f");
                markCustom |= ImGui::DragFloat("Lifetime##PE", &pe.particleLifetime, 0.1f, 0.1f, 100.0f, "%.1f");
                markCustom |= ImGui::DragFloat3("Velocity##PE", &pe.velocity.x, 0.01f, -10.0f, 10.0f, "%.2f");
                markCustom |= ImGui::DragFloat3("Velocity Variance##PE", &pe.velocityVariance.x, 0.01f, 0.0f, 10.0f, "%.2f");
                markCustom |= ImGui::DragFloat3("Spawn Variance##PE", &pe.positionVariance.x, 0.01f, 0.0f, 10.0f, "%.2f");
                markCustom |= ImGui::DragFloat3("Acceleration##PE", &pe.acceleration.x, 0.01f, -10.0f, 10.0f, "%.2f");
                markCustom |= ImGui::DragFloat("Damping##PE", &pe.damping, 0.001f, 0.0f, 1.0f, "%.3f");
                ImGui::SeparatorText("Burst");
                markCustom |= ImGui::Checkbox("Loop Bursts##PE", &pe.looping);
                markCustom |= ImGui::Checkbox("Burst On Start##PE", &pe.burstOnStart);
                markCustom |= ImGui::DragScalar("Burst Count##PE", ImGuiDataType_U32, &pe.burstCount, 1.0f, nullptr, nullptr, "%u");
                markCustom |= ImGui::DragFloat("Burst Interval##PE", &pe.burstInterval, 0.01f, 0.01f, 60.0f, "%.2f s");
                ImGui::SeparatorText("Visuals");
                markCustom |= ImGui::ColorEdit4("Color Start##PE", &pe.colorStart.x);
                markCustom |= ImGui::ColorEdit4("Color Mid##PE", &pe.colorMid.x);
                markCustom |= ImGui::ColorEdit4("Color End##PE", &pe.colorEnd.x);
                markCustom |= ImGui::SliderFloat("Color Midpoint##PE", &pe.colorMidPoint, 0.01f, 0.99f, "%.2f");
                markCustom |= ImGui::DragFloat("Size Start##PE", &pe.sizeStart, 0.01f, 0.01f, 10.0f, "%.2f");
                markCustom |= ImGui::DragFloat("Size Mid##PE", &pe.sizeMid, 0.01f, 0.01f, 10.0f, "%.2f");
                markCustom |= ImGui::DragFloat("Size End##PE", &pe.sizeEnd, 0.01f, 0.01f, 10.0f, "%.2f");
                markCustom |= ImGui::SliderFloat("Size Midpoint##PE", &pe.sizeMidPoint, 0.01f, 0.99f, "%.2f");
                ImGui::SeparatorText("Sprite Sheet");
                markCustom |= ImGui::DragInt("Columns##PE", &pe.spriteColumns, 1.0f, 1, 8);
                markCustom |= ImGui::DragInt("Rows##PE", &pe.spriteRows, 1.0f, 1, 8);
                markCustom |= ImGui::DragFloat("Frame Rate##PE", &pe.spriteFrameRate, 0.25f, 0.0f, 60.0f, "%.2f fps");
                markCustom |= ImGui::Checkbox("Random Start Frame##PE", &pe.randomStartFrame);
                {
                    std::array<char, 512> texturePathBuffer{};
                    std::strncpy(texturePathBuffer.data(), pe.spriteTexturePath.c_str(), texturePathBuffer.size() - 1U);
                    if (ImGui::InputText("Texture Path##PE", texturePathBuffer.data(), texturePathBuffer.size())) {
                        pe.spriteTexturePath = texturePathBuffer.data();
                        markCustom = true;
                    }
                }
                if (ImGui::BeginDragDropTarget()) {
                    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ELYSIUM_ASSET_PATH")) {
                        const auto* droppedPath = static_cast<const char*>(payload->Data);
                        if (droppedPath != nullptr) {
                            pe.spriteTexturePath = droppedPath;
                            markCustom = true;
                        }
                    }
                    ImGui::EndDragDropTarget();
                }
                if (pe.spriteTexturePath.empty()) {
                    ImGui::TextDisabled("No sprite texture set (using procedural fallback).");
                } else {
                    ImGui::TextDisabled("Sprite Texture: %s", pe.spriteTexturePath.c_str());
                }
                ImGui::SeparatorText("Ground Collision");
                markCustom |= ImGui::Checkbox("Enable Collision##PE", &pe.collideWithGround);
                markCustom |= ImGui::DragFloat3("Plane Normal##PE", &pe.collisionPlaneNormal.x, 0.01f, -1.0f, 1.0f, "%.2f");
                markCustom |= ImGui::DragFloat("Plane Offset##PE", &pe.collisionPlaneOffset, 0.01f, -100.0f, 100.0f, "%.2f");
                markCustom |= ImGui::DragFloat("Bounce Factor##PE", &pe.bounceFactor, 0.01f, 0.0f, 1.5f, "%.2f");
                markCustom |= ImGui::DragFloat("Collision Fade##PE", &pe.collisionFade, 0.01f, 0.0f, 1.0f, "%.2f");
                markCustom |= ImGui::DragInt("Max Bounces##PE", &pe.maxBounces, 1.0f, 0, 8);
                if (markCustom && pe.preset != SceneEntity::ParticleEmitterComponent::Preset::Custom) {
                    pe.preset = SceneEntity::ParticleEmitterComponent::Preset::Custom;
                }

                const auto runtimeEmitter = m_particleEmitters.find(selectedEntity->id);
                if (runtimeEmitter != m_particleEmitters.end() && runtimeEmitter->second) {
                    ImGui::Text("Active: %u / %u",
                                runtimeEmitter->second->activeParticleCount(),
                                runtimeEmitter->second->maxParticleCount());
                } else {
                    ImGui::TextDisabled("Runtime emitter inactive");
                }
            }
        }

        ImGui::Separator();
        ImGui::TextUnformatted("Audio");
        if (!selectedEntity->audio.has_value()) {
            if (ImGui::Button("Add Audio Component")) {
                selectedEntity->audio = SceneEntity::AudioComponent{};
            }
        } else {
            if (ImGui::Button("Remove Audio Component")) {
                selectedEntity->audio.reset();
            } else {
                auto& ac = *selectedEntity->audio;
                ImGui::Checkbox("Enabled##AudioComponent", &ac.enabled);
                ImGui::Checkbox("Looping##AudioComponent", &ac.looping);
                ImGui::DragFloat("Gain##AudioComponent", &ac.gain, 0.01f, 0.0f, 4.0f, "%.2f");
                ImGui::DragFloat("Min Distance##AudioComponent", &ac.minDistance, 0.05f, 0.01f, 500.0f, "%.2f");
                ac.maxDistance = std::max(ac.maxDistance, ac.minDistance + 0.01f);
                ImGui::DragFloat("Max Distance##AudioComponent", &ac.maxDistance, 0.1f, ac.minDistance + 0.01f, 5000.0f, "%.2f");

                std::array<char, 512> audioPathBuffer{};
                std::strncpy(audioPathBuffer.data(), ac.clipPath.c_str(), audioPathBuffer.size() - 1U);
                if (ImGui::InputText("Clip Path##AudioComponent", audioPathBuffer.data(), audioPathBuffer.size())) {
                    ac.clipPath = audioPathBuffer.data();
                }

                if (ImGui::SmallButton("Normalize To Asset-Relative##AudioComponent")) {
                    if (ac.clipPath.empty()) {
                        appendLog("Audio clip path is empty; nothing to normalize.");
                    } else {
                        std::error_code ec;
                        std::filesystem::path clipPath{ac.clipPath};
                        if (clipPath.is_relative()) {
                            clipPath = std::filesystem::current_path() / clipPath;
                        }
                        clipPath = std::filesystem::weakly_canonical(clipPath, ec);
                        if (ec) {
                            clipPath = std::filesystem::path{ac.clipPath}.lexically_normal();
                        }

                        std::filesystem::path rootPath = std::filesystem::weakly_canonical(m_assetCatalog.root(), ec);
                        if (ec || rootPath.empty()) {
                            rootPath = m_assetCatalog.root().lexically_normal();
                            ec.clear();
                        }

                        std::filesystem::path rel = clipPath.lexically_relative(rootPath);
                        if (!rel.empty() && rel.native().find("..") != 0) {
                            ac.clipPath = rel.generic_string();
                            appendLog("Normalized audio clip path to asset-relative: " + ac.clipPath);
                        } else {
                            appendLog("Audio clip is outside current asset root; keeping absolute path: " + clipPath.string());
                            ac.clipPath = clipPath.string();
                        }
                    }
                }
                ImGui::SameLine();
                if (ImGui::SmallButton("Make Absolute##AudioComponent")) {
                    if (ac.clipPath.empty()) {
                        appendLog("Audio clip path is empty; nothing to convert.");
                    } else {
                        std::filesystem::path clipPath{ac.clipPath};
                        if (clipPath.is_relative()) {
                            std::filesystem::path base = m_assetCatalog.root();
                            if (base.empty()) {
                                base = std::filesystem::current_path();
                            }
                            clipPath = (base / clipPath).lexically_normal();
                            appendLog("Converted audio clip path to absolute: " + clipPath.string());
                        } else {
                            clipPath = clipPath.lexically_normal();
                            appendLog("Audio clip path is already absolute.");
                        }
                        ac.clipPath = clipPath.string();
                    }
                }

                if (ImGui::BeginDragDropTarget()) {
                    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ELYSIUM_ASSET_PATH")) {
                        const auto* droppedPath = static_cast<const char*>(payload->Data);
                        if (droppedPath != nullptr) {
                            const std::filesystem::path droppedAssetPath{droppedPath};
                            if (isKnownAudioFormat(droppedAssetPath)) {
                                ac.clipPath = droppedAssetPath.string();
                            } else {
                                appendLog("Audio component accepts only audio assets (.wav/.ogg/.mp3/.flac): " + droppedAssetPath.string());
                            }
                        }
                    }
                    ImGui::EndDragDropTarget();
                }

                if (ac.clipPath.empty()) {
                    ImGui::TextDisabled("No clip assigned (spatial submit skipped for this entity).");
                } else {
                    const std::filesystem::path clipPath{ac.clipPath};
                    const bool knownAudio = isKnownAudioFormat(clipPath);
                    const bool backendPlayable = isWavAudioFormat(clipPath);
                    if (!knownAudio) {
                        ImGui::TextColored(ImVec4(1.0f, 0.65f, 0.35f, 1.0f),
                                           "Clip extension is not a supported audio asset type.");
                    } else if (!backendPlayable) {
                        ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.35f, 1.0f),
                                           "Built-in backend currently plays .wav only; this clip may be skipped.");
                    }
                    ImGui::TextDisabled("Clip: %s", ac.clipPath.c_str());
                }
            }
        }

        if (!ImGui::IsAnyItemActive()) {
            m_propertyEditActive = false;
        }
    } else {
        m_selectedEntityId.reset();
        m_propertyEditActive = false;
        ImGui::TextUnformatted("Select a node in Outliner.");
    }

    ImGui::End();
}

void Application::drawConsolePanel() {
    ImGui::Begin("Console");

    drawLuaConsole();
    ImGui::Separator();

    for (const auto& line : m_consoleLines) {
        ImGui::TextUnformatted(line.c_str());
    }
    ImGui::End();
}

void Application::drawLuaConsole() {
    ImGui::TextUnformatted("Lua Console");
    ImGui::SetNextItemWidth(-1.0f);
    const bool submitted = ImGui::InputText(
        "##LuaConsoleInput",
        m_luaConsoleInput.data(),
        m_luaConsoleInput.size(),
        ImGuiInputTextFlags_EnterReturnsTrue
    );

    if (submitted) {
        const std::string code = m_luaConsoleInput.data();
        if (!code.empty()) {
            m_scriptSystem.executeString(code);
            m_luaConsoleInput.fill('\0');
        }
    }

    if (ImGui::Button("Run Lua")) {
        const std::string code = m_luaConsoleInput.data();
        if (!code.empty()) {
            m_scriptSystem.executeString(code);
            m_luaConsoleInput.fill('\0');
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear Logs")) {
        m_consoleLines.clear();
    }
}

void Application::rebuildFonts() {
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->Clear();
    const auto fontPath = std::filesystem::current_path() / "assets/fonts/Roboto-Medium.ttf";
    if (std::filesystem::exists(fontPath)) {
        io.Fonts->AddFontFromFileTTF(fontPath.string().c_str(), m_uiFontSize);
    } else {
        ImFontConfig cfg;
        cfg.SizePixels    = m_uiFontSize;
        cfg.OversampleH   = 1;
        cfg.OversampleV   = 1;
        cfg.PixelSnapH    = true;
        io.Fonts->AddFontDefault(&cfg);
    }
    ImGui_ImplOpenGL3_DestroyDeviceObjects();
    ImGui_ImplOpenGL3_CreateDeviceObjects();
}

void Application::drawSettingsWindow() {
    if (!m_showSettingsWindow) return;
    ImGui::SetNextWindowSize(ImVec2(380.0f, 130.0f), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Settings", &m_showSettingsWindow)) {
        ImGui::SeparatorText("Appearance");
        float size = m_uiFontSize;
        ImGui::SetNextItemWidth(220.0f);
        if (ImGui::SliderFloat("Font Size", &size, 10.0f, 48.0f, "%.0f px")) {
            m_uiFontSize = size;
            m_fontsDirty = true;
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Reset")) {
            m_uiFontSize = 26.0f;
            m_fontsDirty = true;
        }
    }
    ImGui::End();
}

void Application::endImGuiFrame() {
    ImGui::Render();

    int fbW = 0;
    int fbH = 0;
    SDL_GL_GetDrawableSize(m_window, &fbW, &fbH);
    glViewport(0, 0, fbW, fbH);
    glClearColor(0.06f, 0.06f, 0.08f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    ImGuiIO& io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        SDL_Window* backupWindow = SDL_GL_GetCurrentWindow();
        SDL_GLContext backupContext = SDL_GL_GetCurrentContext();
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
        SDL_GL_MakeCurrent(backupWindow, backupContext);
    }

    SDL_GL_SwapWindow(m_window);
}

void Application::appendLog(const std::string& line) {
    m_consoleLines.push_back(line);
    spdlog::info("{}", line);
}

void Application::loadStartupModelIfAny(int argc, char** argv) {
    if (argc <= 1) {
        appendLog("No startup model provided.");
        return;
    }

    std::filesystem::path p{argv[1]};
    if (!std::filesystem::exists(p)) {
        appendLog("Startup path does not exist: " + p.string());
        return;
    }

    instantiateModel(p);
}

void Application::instantiateModel(const std::filesystem::path& filePath) {
    if (!isRuntimeModelFormat(filePath)) {
        if (isSourceModelFormat(filePath)) {
            appendLog("Source model format requires conversion to .glb/.gltf before runtime use: " + filePath.string());
        } else {
            appendLog("Unsupported runtime model format: " + filePath.string());
        }
        return;
    }

    captureUndoSnapshot("Instantiate model");

    const std::string key = filePath.lexically_normal().string();

    std::shared_ptr<GLTFModel> model;
    auto it = m_modelCache.find(key);
    if (it != m_modelCache.end()) {
        model = it->second;
    } else {
        model = m_loader.loadModelFromFile(filePath);
        if (!model) {
            appendLog("Failed to load: " + key);
            return;
        }
        m_modelCache.emplace(key, model);
    }

    const SceneEntityId entityId = m_scene.createEntity(filePath.stem().string(), model);
    SceneEntity* entity = m_scene.findEntity(entityId);
    if (entity == nullptr) {
        appendLog("Internal error: failed to create scene entity for model: " + key);
        return;
    }

    entity->sourceAssetPath = key;

    if (m_snapNewPlacements) {
        snapEntityToGrid(*entity);
    }

    m_selectedEntityId = entityId;
    appendLog("Instantiated model: " + key);
}

void Application::handleDroppedPath(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path)) {
        appendLog("Dropped path does not exist: " + path.string());
        return;
    }

    if (std::filesystem::is_directory(path)) {
        std::string error;
        if (m_assetCatalog.setRoot(path, &error)) {
            clearAssetThumbnailCache();
            const std::string rootString = m_assetCatalog.root().string();
            m_assetRootInput.fill('\0');
            std::strncpy(m_assetRootInput.data(), rootString.c_str(), m_assetRootInput.size() - 1U);
            m_scriptSystem.setScriptRoot(m_assetCatalog.root());
            appendLog("Asset root changed to dropped folder: " + rootString);
        } else {
            appendLog("Asset root error: " + error);
        }
        return;
    }

    if (isRuntimeModelFormat(path)) {
        instantiateModel(path);
    } else if (isSourceModelFormat(path)) {
        appendLog("Dropped source model format is not runtime-loadable; export to .glb/.gltf: " + path.string());
    } else {
        appendLog("Ignored dropped file (not a model asset): " + path.string());
    }
}

void Application::snapEntityToGrid(SceneEntity& entity) const {
    if (m_gridSnapSize <= 0.0f) {
        return;
    }

    const float s = m_gridSnapSize;
    entity.transform.position.x = std::round(entity.transform.position.x / s) * s;
    entity.transform.position.y = std::round(entity.transform.position.y / s) * s;
    entity.transform.position.z = std::round(entity.transform.position.z / s) * s;
}

bool Application::loadSplashTexture(const std::filesystem::path& filePath) {
    int width = 0;
    int height = 0;
    int channels = 0;
    unsigned char* srcPixels = stbi_load(filePath.string().c_str(), &width, &height, &channels, 0);
    if (srcPixels == nullptr || width <= 0 || height <= 0) {
        spdlog::warn("Failed to load splash image: {}", filePath.string());
        return false;
    }

    // Always upload RGBA. For images without alpha (e.g., JPG), key out the
    // dominant corner color to produce a transparent background fallback.
    std::vector<unsigned char> rgbaPixels(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4U, 255U);

    if (channels >= 4) {
        for (int i = 0; i < width * height; ++i) {
            rgbaPixels[static_cast<std::size_t>(i) * 4U + 0U] = srcPixels[static_cast<std::size_t>(i) * static_cast<std::size_t>(channels) + 0U];
            rgbaPixels[static_cast<std::size_t>(i) * 4U + 1U] = srcPixels[static_cast<std::size_t>(i) * static_cast<std::size_t>(channels) + 1U];
            rgbaPixels[static_cast<std::size_t>(i) * 4U + 2U] = srcPixels[static_cast<std::size_t>(i) * static_cast<std::size_t>(channels) + 2U];
            rgbaPixels[static_cast<std::size_t>(i) * 4U + 3U] = srcPixels[static_cast<std::size_t>(i) * static_cast<std::size_t>(channels) + 3U];
        }
    } else {
        const unsigned char keyR = srcPixels[0];
        const unsigned char keyG = channels > 1 ? srcPixels[1] : srcPixels[0];
        const unsigned char keyB = channels > 2 ? srcPixels[2] : srcPixels[0];

        for (int i = 0; i < width * height; ++i) {
            const std::size_t srcBase = static_cast<std::size_t>(i) * static_cast<std::size_t>(channels);
            const unsigned char r = srcPixels[srcBase + 0U];
            const unsigned char g = channels > 1 ? srcPixels[srcBase + 1U] : srcPixels[srcBase + 0U];
            const unsigned char b = channels > 2 ? srcPixels[srcBase + 2U] : srcPixels[srcBase + 0U];

            const int dr = std::abs(static_cast<int>(r) - static_cast<int>(keyR));
            const int dg = std::abs(static_cast<int>(g) - static_cast<int>(keyG));
            const int db = std::abs(static_cast<int>(b) - static_cast<int>(keyB));
            const int colorDistance = dr + dg + db;

            rgbaPixels[static_cast<std::size_t>(i) * 4U + 0U] = r;
            rgbaPixels[static_cast<std::size_t>(i) * 4U + 1U] = g;
            rgbaPixels[static_cast<std::size_t>(i) * 4U + 2U] = b;
            rgbaPixels[static_cast<std::size_t>(i) * 4U + 3U] = colorDistance < 24 ? 0U : 255U;
        }
    }

    destroySplashTexture();

    glCreateTextures(GL_TEXTURE_2D, 1, &m_splashTexture);
    glTextureStorage2D(m_splashTexture, 1, GL_RGBA8, width, height);
    glTextureSubImage2D(m_splashTexture, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, rgbaPixels.data());
    glTextureParameteri(m_splashTexture, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTextureParameteri(m_splashTexture, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    stbi_image_free(srcPixels);

    m_splashWidth = width;
    m_splashHeight = height;
    return true;
}

void Application::destroySplashTexture() {
    if (m_splashTexture != 0) {
        glDeleteTextures(1, &m_splashTexture);
        m_splashTexture = 0;
    }
    m_splashWidth = 0;
    m_splashHeight = 0;
}

bool Application::showStartupSplash(std::chrono::milliseconds duration) {
    using clock = std::chrono::steady_clock;
    const auto startTime = clock::now();
    const float totalSeconds = std::chrono::duration<float>(duration).count();
    const float fadeInSeconds = std::min(0.35f, totalSeconds * 0.33f);
    const float fadeOutSeconds = std::min(0.30f, totalSeconds * 0.25f);

    while (clock::now() - startTime < duration) {
        const float elapsedSeconds = std::chrono::duration<float>(clock::now() - startTime).count();
        float alpha = 1.0f;
        if (elapsedSeconds < fadeInSeconds && fadeInSeconds > 0.0f) {
            alpha = elapsedSeconds / fadeInSeconds;
        } else if (elapsedSeconds > totalSeconds - fadeOutSeconds && fadeOutSeconds > 0.0f) {
            alpha = (totalSeconds - elapsedSeconds) / fadeOutSeconds;
        }
        alpha = std::clamp(alpha, 0.0f, 1.0f);

        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            ImGui_ImplSDL2_ProcessEvent(&ev);
            if (ev.type == SDL_QUIT || (ev.type == SDL_WINDOWEVENT && ev.window.event == SDL_WINDOWEVENT_CLOSE)) {
                m_running = false;
                return false;
            }
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->Pos);
        ImGui::SetNextWindowSize(viewport->Size);
        ImGui::SetNextWindowViewport(viewport->ID);

        ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                 ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoInputs |
                                 ImGuiWindowFlags_NoBackground;
        ImGui::Begin("SplashScreen", nullptr, flags);

        const ImVec2 avail = ImGui::GetContentRegionAvail();
        const float maxWidth = avail.x * 0.55f;
        const float maxHeight = avail.y * 0.55f;
        const float scale = std::min(maxWidth / static_cast<float>(m_splashWidth), maxHeight / static_cast<float>(m_splashHeight));
        const float drawW = static_cast<float>(m_splashWidth) * scale;
        const float drawH = static_cast<float>(m_splashHeight) * scale;
        const float pulse = 0.5f + 0.5f * std::sin(elapsedSeconds * 4.0f);

        ImGui::SetCursorPosX((avail.x - drawW) * 0.5f);
        ImGui::SetCursorPosY((avail.y - drawH) * 0.42f);
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);
        // Rotate splash texture 180 degrees relative to the default OpenGL-flipped UV mapping.
        ImGui::Image(
            static_cast<ImTextureID>(static_cast<uintptr_t>(m_splashTexture)),
            ImVec2(drawW, drawH),
            ImVec2(1, 1),
            ImVec2(0, 0)
        );

        const char* loadingLabel = "Loading Elysium Engine...";
        const ImVec2 textSize = ImGui::CalcTextSize(loadingLabel);
        ImGui::SetCursorPosX((avail.x - textSize.x) * 0.5f);
        ImGui::SetCursorPosY((avail.y - drawH) * 0.42f + drawH + 18.0f);
        ImGui::TextColored(ImVec4(0.86f, 0.89f, 0.94f, alpha), "%s", loadingLabel);

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const float barWidth = std::min(320.0f, avail.x * 0.34f);
        const float barHeight = 6.0f;
        const ImVec2 barMin(
            viewport->Pos.x + (avail.x - barWidth) * 0.5f,
            viewport->Pos.y + (avail.y - drawH) * 0.42f + drawH + 48.0f
        );
        const ImVec2 barMax(barMin.x + barWidth, barMin.y + barHeight);
        const float progress = std::clamp(elapsedSeconds / totalSeconds, 0.0f, 1.0f);
        const ImVec2 fillMax(barMin.x + barWidth * (0.15f + progress * 0.85f), barMax.y);

        drawList->AddRectFilled(barMin, barMax, IM_COL32(36, 42, 50, static_cast<int>(alpha * 220.0f)), 3.0f);
        drawList->AddRectFilled(
            barMin,
            fillMax,
            IM_COL32(
                static_cast<int>(96.0f + pulse * 40.0f),
                static_cast<int>(134.0f + pulse * 40.0f),
                static_cast<int>(164.0f + pulse * 28.0f),
                static_cast<int>(alpha * 235.0f)
            ),
            3.0f
        );
        drawList->AddRect(barMin, barMax, IM_COL32(96, 108, 124, static_cast<int>(alpha * 255.0f)), 3.0f);
        ImGui::PopStyleVar();

        ImGui::End();

        ImGui::Render();
        int fbW = 0;
        int fbH = 0;
        SDL_GL_GetDrawableSize(m_window, &fbW, &fbH);
        glViewport(0, 0, fbW, fbH);
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        ImGuiIO& io = ImGui::GetIO();
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
            SDL_Window* backupWindow = SDL_GL_GetCurrentWindow();
            SDL_GLContext backupContext = SDL_GL_GetCurrentContext();
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            SDL_GL_MakeCurrent(backupWindow, backupContext);
        }

        SDL_GL_SwapWindow(m_window);
    }

    return true;
}

void Application::handleViewportShortcuts() {
    if (!ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) {
        return;
    }

    // Gizmo mode shortcuts (disabled during character preview to free WASD).
    if (!m_walkmeshPreviewEnabled) {
        if (ImGui::IsKeyPressed(ImGuiKey_W)) m_gizmoMode = GizmoMode::Translate;
        if (ImGui::IsKeyPressed(ImGuiKey_E)) m_gizmoMode = GizmoMode::Rotate;
        if (ImGui::IsKeyPressed(ImGuiKey_R)) m_gizmoMode = GizmoMode::Scale;
    }

    // Toggle character preview mode.
    if (ImGui::IsKeyPressed(ImGuiKey_P)) {
        m_walkmeshPreviewEnabled = !m_walkmeshPreviewEnabled;
        if (m_walkmeshPreviewEnabled) {
            rebuildWalkmesh();
            glm::vec3 startPos{0.0f};
            const auto h = m_tileWalkmesh.sampleHeight(0.0f, 0.0f);
            if (h.has_value()) startPos.y = h.value();
            m_controller.reset(startPos);
        }
    }
}

void Application::handleViewportSelectionAndGizmo(const ImVec2& imageMin, const ImVec2& imageMax) {
    ImGuiIO& io = ImGui::GetIO();
    const bool hovered = io.MousePos.x >= imageMin.x && io.MousePos.x <= imageMax.x &&
                         io.MousePos.y >= imageMin.y && io.MousePos.y <= imageMax.y;

    SceneEntity* selectedEntity = m_selectedEntityId.has_value() ? m_scene.findEntity(m_selectedEntityId.value()) : nullptr;
    if (selectedEntity == nullptr) {
        m_activeCollisionPlaneHandle = CollisionPlaneHandle::None;
    }

    if (selectedEntity != nullptr && selectedEntity->particleEmitter.has_value() && selectedEntity->particleEmitter->collideWithGround) {
        const float gizmoExtent = std::max(selectedEntity->transform.scale.x, std::max(selectedEntity->transform.scale.y, selectedEntity->transform.scale.z));
        const float gizmoLength = std::max(0.85f, gizmoExtent + 0.35f);
        CollisionPlaneOverlay planeOverlay{};
        if (buildCollisionPlaneOverlay(*selectedEntity, m_viewport, gizmoLength, imageMin, imageMax, planeOverlay) &&
            planeOverlay.centerProjected && planeOverlay.tipProjected) {
            if (m_activeCollisionPlaneHandle != CollisionPlaneHandle::None) {
                if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                    m_activeCollisionPlaneHandle = CollisionPlaneHandle::None;
                } else {
                    const float dx = io.MousePos.x - m_lastCollisionPlaneMousePos.x;
                    const float dy = io.MousePos.y - m_lastCollisionPlaneMousePos.y;
                    auto& pe = *selectedEntity->particleEmitter;

                    if (m_activeCollisionPlaneHandle == CollisionPlaneHandle::Offset) {
                        pe.collisionPlaneOffset += (-dy) * 0.01f * std::max(1.0f, gizmoLength);
                    } else if (m_activeCollisionPlaneHandle == CollisionPlaneHandle::Normal) {
                        glm::vec3 normal = pe.collisionPlaneNormal;
                        const float lenSq = glm::dot(normal, normal);
                        if (lenSq <= 1e-4f) {
                            normal = glm::vec3(0.0f, 1.0f, 0.0f);
                        } else {
                            normal = glm::normalize(normal);
                        }

                        const float yawRad = glm::radians(m_viewport.cameraYawDeg());
                        const glm::vec3 rightXZ{std::cos(yawRad), 0.0f, std::sin(yawRad)};
                        normal += rightXZ * (dx * 0.005f) + glm::vec3(0.0f, 1.0f, 0.0f) * (-dy * 0.005f);
                        const float newLenSq = glm::dot(normal, normal);
                        if (newLenSq > 1e-4f) {
                            pe.collisionPlaneNormal = glm::normalize(normal);
                        }
                    }

                    m_lastCollisionPlaneMousePos = io.MousePos;
                }
                return;
            }

            if (hovered && !m_tileBrushEnabled && !io.KeyAlt && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                const float centerDx = io.MousePos.x - planeOverlay.centerScreen.x;
                const float centerDy = io.MousePos.y - planeOverlay.centerScreen.y;
                const float centerDistSq = centerDx * centerDx + centerDy * centerDy;
                if (centerDistSq <= 11.0f * 11.0f) {
                    captureUndoSnapshot("Adjust particle collision plane");
                    m_activeCollisionPlaneHandle = CollisionPlaneHandle::Offset;
                    m_lastCollisionPlaneMousePos = io.MousePos;
                    return;
                }

                const float tipDx = io.MousePos.x - planeOverlay.tipScreen.x;
                const float tipDy = io.MousePos.y - planeOverlay.tipScreen.y;
                const float tipDistSq = tipDx * tipDx + tipDy * tipDy;
                if (tipDistSq <= 11.0f * 11.0f) {
                    captureUndoSnapshot("Rotate particle collision plane");
                    m_activeCollisionPlaneHandle = CollisionPlaneHandle::Normal;
                    m_lastCollisionPlaneMousePos = io.MousePos;
                    return;
                }
            }
        }
    }

    if (m_activeGizmoAxis != GizmoAxis::None) {
        if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            m_activeGizmoAxis = GizmoAxis::None;
            return;
        }

        if (selectedEntity == nullptr) {
            m_activeGizmoAxis = GizmoAxis::None;
            return;
        }

        if (m_gizmoMode == GizmoMode::Translate) {
            if (m_activeGizmoAxis == GizmoAxis::Y) {
                selectedEntity->transform.position.y -= (io.MousePos.y - m_lastGizmoMousePos.y) * 0.01f;
                m_lastGizmoMousePos = io.MousePos;
            } else {
                glm::vec3 currentGroundPoint{0.0f};
                if (m_viewport.screenPointToGround(io.MousePos, imageMin, imageMax, currentGroundPoint)) {
                    const glm::vec3 delta = currentGroundPoint - m_lastGizmoGroundPoint;
                    const glm::vec3 axis = gizmoAxisVector(m_activeGizmoAxis);
                    selectedEntity->transform.position += axis * glm::dot(delta, axis);
                    m_lastGizmoGroundPoint = currentGroundPoint;
                }
            }
        } else if (m_gizmoMode == GizmoMode::Rotate) {
            const float delta = (io.MousePos.x - m_lastGizmoMousePos.x) * 0.35f;
            if (m_activeGizmoAxis == GizmoAxis::X) selectedEntity->transform.rotationEulerDeg.x += delta;
            if (m_activeGizmoAxis == GizmoAxis::Y) selectedEntity->transform.rotationEulerDeg.y += delta;
            if (m_activeGizmoAxis == GizmoAxis::Z) selectedEntity->transform.rotationEulerDeg.z += delta;
            m_lastGizmoMousePos = io.MousePos;
        } else if (m_gizmoMode == GizmoMode::Scale) {
            const float delta = (io.MousePos.x - m_lastGizmoMousePos.x) * 0.01f;
            if (m_activeGizmoAxis == GizmoAxis::X) selectedEntity->transform.scale.x = std::max(0.05f, selectedEntity->transform.scale.x + delta);
            if (m_activeGizmoAxis == GizmoAxis::Y) selectedEntity->transform.scale.y = std::max(0.05f, selectedEntity->transform.scale.y + delta);
            if (m_activeGizmoAxis == GizmoAxis::Z) selectedEntity->transform.scale.z = std::max(0.05f, selectedEntity->transform.scale.z + delta);
            m_lastGizmoMousePos = io.MousePos;
        }

        return;
    }

    if (!hovered || m_tileBrushEnabled || io.KeyAlt || !ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        return;
    }

    if (m_selectedEntityId.has_value()) {
        SceneEntity* selectedEntity = m_scene.findEntity(m_selectedEntityId.value());
        if (selectedEntity != nullptr) {
            const glm::vec3 origin = selectedEntity->transform.position;
            const float gizmoExtent = std::max(selectedEntity->transform.scale.x, std::max(selectedEntity->transform.scale.y, selectedEntity->transform.scale.z));
            const float gizmoLength = std::max(0.85f, gizmoExtent + 0.35f);

            ImVec2 originScreen{};
            if (m_viewport.worldToScreen(origin, imageMin, imageMax, originScreen)) {
                for (GizmoAxis axis : {GizmoAxis::X, GizmoAxis::Y, GizmoAxis::Z}) {
                    ImVec2 axisEnd{};
                    if (!m_viewport.worldToScreen(origin + gizmoAxisVector(axis) * gizmoLength, imageMin, imageMax, axisEnd)) {
                        continue;
                    }

                    if (distanceToSegment(io.MousePos, originScreen, axisEnd) <= 9.0f) {
                        captureUndoSnapshot("Gizmo transform");
                        m_activeGizmoAxis = axis;
                        m_lastGizmoMousePos = io.MousePos;
                        m_viewport.screenPointToGround(io.MousePos, imageMin, imageMax, m_lastGizmoGroundPoint);
                        return;
                    }
                }
            }
        }
    }

    float bestDistanceSq = std::numeric_limits<float>::max();
    std::optional<SceneEntityId> bestId;
    for (const auto& entity : m_scene.entities()) {
        ImVec2 screenPoint{};
        if (!m_viewport.worldToScreen(entity.transform.position, imageMin, imageMax, screenPoint)) {
            continue;
        }

        const float dx = screenPoint.x - io.MousePos.x;
        const float dy = screenPoint.y - io.MousePos.y;
        const float distanceSq = dx * dx + dy * dy;
        if (distanceSq < bestDistanceSq && distanceSq <= (18.0f * 18.0f)) {
            bestDistanceSq = distanceSq;
            bestId = entity.id;
        }
    }

    m_selectedEntityId = bestId;
}

void Application::drawViewportGizmoOverlay(const ImVec2& imageMin, const ImVec2& imageMax) {
    SceneEntity* selectedEntity = m_selectedEntityId.has_value() ? m_scene.findEntity(m_selectedEntityId.value()) : nullptr;
    if (selectedEntity == nullptr) {
        return;
    }

    ImVec2 originScreen{};
    if (!m_viewport.worldToScreen(selectedEntity->transform.position, imageMin, imageMax, originScreen)) {
        return;
    }

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->AddCircleFilled(originScreen, 5.0f, IM_COL32(250, 250, 250, 255));
    drawList->AddCircle(originScreen, 9.0f, IM_COL32(255, 255, 255, 180), 0, 1.5f);

    const float gizmoExtent = std::max(selectedEntity->transform.scale.x, std::max(selectedEntity->transform.scale.y, selectedEntity->transform.scale.z));
    const float gizmoLength = std::max(0.85f, gizmoExtent + 0.35f);
    for (GizmoAxis axis : {GizmoAxis::X, GizmoAxis::Y, GizmoAxis::Z}) {
        ImVec2 axisEnd{};
        if (!m_viewport.worldToScreen(selectedEntity->transform.position + gizmoAxisVector(axis) * gizmoLength, imageMin, imageMax, axisEnd)) {
            continue;
        }

        const bool active = m_activeGizmoAxis == axis;
        drawList->AddLine(originScreen, axisEnd, gizmoAxisColor(axis, active), active ? 4.0f : 2.5f);
        drawList->AddCircleFilled(axisEnd, active ? 6.5f : 5.0f, gizmoAxisColor(axis, active));
    }

    if (selectedEntity->particleEmitter.has_value() && selectedEntity->particleEmitter->collideWithGround) {
        CollisionPlaneOverlay planeOverlay{};
        if (buildCollisionPlaneOverlay(*selectedEntity, m_viewport, gizmoLength, imageMin, imageMax, planeOverlay)) {
            if (planeOverlay.cornersProjected) {
                drawList->AddPolyline(planeOverlay.screenCorners.data(), static_cast<int>(planeOverlay.screenCorners.size()), IM_COL32(66, 226, 255, 220), ImDrawFlags_Closed, 2.0f);
                drawList->AddLine(planeOverlay.screenCorners[0], planeOverlay.screenCorners[2], IM_COL32(66, 226, 255, 120), 1.0f);
                drawList->AddLine(planeOverlay.screenCorners[1], planeOverlay.screenCorners[3], IM_COL32(66, 226, 255, 120), 1.0f);
            }

            if (planeOverlay.centerProjected && planeOverlay.tipProjected) {
                drawList->AddLine(planeOverlay.centerScreen, planeOverlay.tipScreen, IM_COL32(90, 255, 140, 240), 2.5f);

                const ImU32 centerColor = m_activeCollisionPlaneHandle == CollisionPlaneHandle::Offset
                    ? IM_COL32(255, 230, 125, 255)
                    : IM_COL32(110, 210, 255, 245);
                const ImU32 tipColor = m_activeCollisionPlaneHandle == CollisionPlaneHandle::Normal
                    ? IM_COL32(255, 230, 125, 255)
                    : IM_COL32(90, 255, 140, 255);

                drawList->AddCircleFilled(planeOverlay.centerScreen, 5.0f, centerColor);
                drawList->AddCircle(planeOverlay.centerScreen, 8.0f, IM_COL32(210, 240, 255, 180), 0, 1.0f);
                drawList->AddCircleFilled(planeOverlay.tipScreen, 5.0f, tipColor);
                drawList->AddCircle(planeOverlay.tipScreen, 8.0f, IM_COL32(210, 240, 255, 180), 0, 1.0f);
                drawList->AddText(
                    ImVec2(planeOverlay.tipScreen.x + 8.0f, planeOverlay.tipScreen.y - 18.0f),
                    IM_COL32(135, 255, 170, 235),
                    "Collision Plane (drag center/tip)");
            }
        }
    }

    drawList->AddText(
        ImVec2(originScreen.x + 12.0f, originScreen.y - 22.0f),
        IM_COL32(235, 235, 235, 255),
        selectedEntity->name.c_str()
    );
}

void Application::drawProceduralPreviewOverlay(const ImVec2& imageMin, const ImVec2& imageMax) {
    if (!m_proceduralOverlayEnabled || !m_proceduralPreviewArea.has_value()) {
        return;
    }

    const GeneratedArea& preview = *m_proceduralPreviewArea;
    if (preview.tiles.empty()) {
        return;
    }

    const float half = m_tileMap.tileSize() * 0.5f;
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    for (const auto& tile : preview.tiles) {
        const glm::vec3 center = m_tileMap.cellToWorldCenter(tile.x, tile.z);
        const glm::vec3 c0(center.x - half, 0.01f, center.z - half);
        const glm::vec3 c1(center.x + half, 0.01f, center.z - half);
        const glm::vec3 c2(center.x + half, 0.01f, center.z + half);
        const glm::vec3 c3(center.x - half, 0.01f, center.z + half);

        ImVec2 p0{};
        ImVec2 p1{};
        ImVec2 p2{};
        ImVec2 p3{};
        if (!m_viewport.worldToScreen(c0, imageMin, imageMax, p0) ||
            !m_viewport.worldToScreen(c1, imageMin, imageMax, p1) ||
            !m_viewport.worldToScreen(c2, imageMin, imageMax, p2) ||
            !m_viewport.worldToScreen(c3, imageMin, imageMax, p3)) {
            continue;
        }

        const glm::vec4 tint = proceduralTintForTile(m_proceduralTileGenerator, tile);
        const ImU32 fill = IM_COL32(
            static_cast<int>(std::clamp(tint.r, 0.0f, 1.0f) * 255.0f),
            static_cast<int>(std::clamp(tint.g, 0.0f, 1.0f) * 255.0f),
            static_cast<int>(std::clamp(tint.b, 0.0f, 1.0f) * 255.0f),
            86);
        const ImU32 border = tile.walkable ? IM_COL32(220, 236, 248, 110) : IM_COL32(248, 132, 132, 170);

        const ImVec2 corners[4] = {p0, p1, p2, p3};
        drawList->AddConvexPolyFilled(corners, 4, fill);
        drawList->AddPolyline(corners, 4, border, ImDrawFlags_Closed, 1.0f);
    }
}

void Application::regenerateProceduralPreview() {
    m_proceduralParams.width = std::max(1, m_proceduralParams.width);
    m_proceduralParams.height = std::max(1, m_proceduralParams.height);
    m_proceduralParams.beachLevel = std::max(m_proceduralParams.beachLevel, m_proceduralParams.waterLevel);

    m_proceduralPreviewArea = m_proceduralTileGenerator.generateArea(m_proceduralParams);
    appendLog(
        "Generated procedural preview: " +
        std::to_string(m_proceduralParams.width) + "x" + std::to_string(m_proceduralParams.height) +
        " seed=" + std::to_string(m_proceduralParams.seed));
}

void Application::applyProceduralPreviewToTileMap() {
    if (!m_proceduralPreviewArea.has_value() || m_proceduralPreviewArea->tiles.empty()) {
        appendLog("Procedural apply skipped: no preview area.");
        return;
    }

    captureUndoSnapshot("Apply procedural preview");

    m_lastAppliedProceduralRestore.clear();
    m_lastAppliedProceduralRestore.reserve(m_proceduralPreviewArea->tiles.size());
    const auto& existingTiles = m_tileMap.tiles();
    for (const auto& tile : m_proceduralPreviewArea->tiles) {
        ProceduralTileRestore restore{};
        restore.x = tile.x;
        restore.z = tile.z;

        const TileCoord coord{tile.x, tile.z};
        const auto it = existingTiles.find(coord);
        if (it != existingTiles.end()) {
            restore.hadPreviousTile = true;
            restore.previousTile = it->second;
        }

        m_lastAppliedProceduralRestore.push_back(restore);
    }

    m_proceduralTileGenerator.applyAreaToTileMap(*m_proceduralPreviewArea, m_tileMap);
    m_walkmeshDirty = true;
    appendLog("Applied procedural preview to tilemap: " + std::to_string(m_proceduralPreviewArea->tiles.size()) + " tiles.");
}

void Application::clearProceduralPreview() {
    if (!m_proceduralPreviewArea.has_value()) {
        return;
    }
    m_proceduralPreviewArea.reset();
    appendLog("Cleared procedural preview.");
}

void Application::clearLastAppliedProceduralArea() {
    if (m_lastAppliedProceduralRestore.empty()) {
        appendLog("Clear applied procedural skipped: nothing to clear.");
        return;
    }

    captureUndoSnapshot("Clear applied procedural area");
    for (const auto& restore : m_lastAppliedProceduralRestore) {
        if (restore.hadPreviousTile) {
            m_tileMap.placeTile(restore.x, restore.z, restore.previousTile);
        } else {
            m_tileMap.eraseTile(restore.x, restore.z);
        }
    }

    appendLog("Cleared last applied procedural area: " + std::to_string(m_lastAppliedProceduralRestore.size()) + " tiles restored.");
    m_lastAppliedProceduralRestore.clear();
    m_walkmeshDirty = true;
}

Application::EditorStateSnapshot Application::makeSnapshot() const {
    EditorStateSnapshot snapshot{};
    snapshot.tileSize = m_tileMap.tileSize();
    snapshot.gridEnabled = m_gridEnabled;
    snapshot.snapNewPlacements = m_snapNewPlacements;
    snapshot.tileBrushEnabled = m_tileBrushEnabled;
    snapshot.brushWalkable = m_brushWalkable;
    snapshot.tileBrushTint = m_tileBrushTint;

    snapshot.entities.reserve(m_scene.entities().size());
    for (const auto& entity : m_scene.entities()) {
        SavedEntity saved{};
        saved.name = entity.name;
        saved.assetPath = entity.sourceAssetPath;
        saved.scriptEnabled = entity.script.has_value() ? entity.script->enabled : false;
        saved.scriptPath = entity.script.has_value() ? entity.script->scriptPath : std::string{};
        saved.physicsEnabled = entity.physics.has_value() ? entity.physics->enabled : false;
        saved.physicsDynamic = entity.physics.has_value() ? entity.physics->dynamic : true;
        saved.physicsHalfExtents = entity.physics.has_value() ? entity.physics->halfExtents : glm::vec3(0.5f, 0.5f, 0.5f);
        saved.transform = entity.transform;
        saved.tint = entity.tint;
        saved.hasLight = entity.light.has_value();
        if (entity.light.has_value()) {
            saved.lightType      = static_cast<int>(entity.light->type);
            saved.lightEnabled   = entity.light->enabled;
            saved.lightColor     = entity.light->color;
            saved.lightIntensity = entity.light->intensity;
            saved.lightRange     = entity.light->range;
        }
        saved.hasParticleEmitter = entity.particleEmitter.has_value();
        if (entity.particleEmitter.has_value()) {
            const auto& pe = *entity.particleEmitter;
            saved.particleEmitterEnabled = pe.enabled;
            saved.particleEmitterPreset = static_cast<int>(pe.preset);
            saved.particleEmitterBlendMode = static_cast<int>(pe.blendMode);
            saved.particleEmitterMaxParticles = pe.maxParticles;
            saved.particleEmitterEmissionRate = pe.emissionRate;
            saved.particleEmitterLifetime = pe.particleLifetime;
            saved.particleEmitterVelocity = pe.velocity;
            saved.particleEmitterVelocityVariance = pe.velocityVariance;
            saved.particleEmitterPositionVariance = pe.positionVariance;
            saved.particleEmitterAcceleration = pe.acceleration;
            saved.particleEmitterDamping = pe.damping;
            saved.particleEmitterLooping = pe.looping;
            saved.particleEmitterBurstCount = pe.burstCount;
            saved.particleEmitterBurstInterval = pe.burstInterval;
            saved.particleEmitterBurstOnStart = pe.burstOnStart;
            saved.particleEmitterColorStart = pe.colorStart;
            saved.particleEmitterColorMid = pe.colorMid;
            saved.particleEmitterColorEnd = pe.colorEnd;
            saved.particleEmitterColorMidPoint = pe.colorMidPoint;
            saved.particleEmitterSizeStart = pe.sizeStart;
            saved.particleEmitterSizeMid = pe.sizeMid;
            saved.particleEmitterSizeEnd = pe.sizeEnd;
            saved.particleEmitterSizeMidPoint = pe.sizeMidPoint;
            saved.particleEmitterSpriteColumns = pe.spriteColumns;
            saved.particleEmitterSpriteRows = pe.spriteRows;
            saved.particleEmitterSpriteFrameRate = pe.spriteFrameRate;
            saved.particleEmitterRandomStartFrame = pe.randomStartFrame;
            saved.particleEmitterSpriteTexturePath = pe.spriteTexturePath;
            saved.particleEmitterCollideWithGround = pe.collideWithGround;
            saved.particleEmitterCollisionPlaneNormal = pe.collisionPlaneNormal;
            saved.particleEmitterCollisionPlaneOffset = pe.collisionPlaneOffset;
            saved.particleEmitterBounceFactor = pe.bounceFactor;
            saved.particleEmitterCollisionFade = pe.collisionFade;
            saved.particleEmitterMaxBounces = pe.maxBounces;
        }
        saved.hasAudio = entity.audio.has_value();
        if (entity.audio.has_value()) {
            const auto& ac = *entity.audio;
            saved.audioEnabled = ac.enabled;
            saved.audioClipPath = ac.clipPath;
            saved.audioGain = ac.gain;
            saved.audioMinDistance = ac.minDistance;
            saved.audioMaxDistance = ac.maxDistance;
            saved.audioLooping = ac.looping;
        }
        snapshot.entities.push_back(std::move(saved));
    }

    snapshot.tiles.reserve(m_tileMap.tiles().size());
    for (const auto& [coord, tile] : m_tileMap.tiles()) {
        SavedTile savedTile{};
        savedTile.x = coord.x;
        savedTile.z = coord.z;
        savedTile.data = tile;
        snapshot.tiles.push_back(savedTile);
    }
    std::sort(snapshot.tiles.begin(), snapshot.tiles.end(), [](const SavedTile& a, const SavedTile& b) {
        if (a.x != b.x) {
            return a.x < b.x;
        }
        return a.z < b.z;
    });

    return snapshot;
}

bool Application::snapshotsEqual(const EditorStateSnapshot& lhs, const EditorStateSnapshot& rhs) const {
    if (!nearlyEqual(lhs.tileSize, rhs.tileSize)) return false;
    if (lhs.gridEnabled != rhs.gridEnabled) return false;
    if (lhs.snapNewPlacements != rhs.snapNewPlacements) return false;
    if (lhs.tileBrushEnabled != rhs.tileBrushEnabled) return false;
    if (lhs.brushWalkable != rhs.brushWalkable) return false;
    if (lhs.tileBrushTint != rhs.tileBrushTint) return false;
    if (lhs.entities != rhs.entities) return false;
    if (lhs.tiles != rhs.tiles) return false;
    return true;
}

void Application::applySnapshot(const EditorStateSnapshot& snapshot) {
    m_suppressHistory = true;
    m_proceduralPreviewArea.reset();
    m_lastAppliedProceduralRestore.clear();

    m_scene.clear();
    m_tileMap.clear();
    m_selectedEntityId.reset();

    m_gridSnapSize = snapshot.tileSize;
    m_tileMap.setTileSize(snapshot.tileSize);
    m_gridEnabled = snapshot.gridEnabled;
    m_snapNewPlacements = snapshot.snapNewPlacements;
    m_tileBrushEnabled = snapshot.tileBrushEnabled;
    m_brushWalkable = snapshot.brushWalkable;
    m_tileBrushTint = snapshot.tileBrushTint;

    for (const auto& tile : snapshot.tiles) {
        m_tileMap.placeTile(tile.x, tile.z, tile.data);
    }

    for (const auto& saved : snapshot.entities) {
        std::shared_ptr<GLTFModel> model;
        if (!saved.assetPath.empty()) {
            const auto it = m_modelCache.find(saved.assetPath);
            if (it != m_modelCache.end()) {
                model = it->second;
            } else {
                const std::filesystem::path savedPath{saved.assetPath};
                if (isRuntimeModelFormat(savedPath)) {
                    model = m_loader.loadModelFromFile(savedPath);
                    if (model) {
                        m_modelCache.emplace(saved.assetPath, model);
                    }
                } else if (isSourceModelFormat(savedPath)) {
                    appendLog("Snapshot restore skipped source model format (convert to .glb/.gltf): " + saved.assetPath);
                } else {
                    appendLog("Snapshot restore skipped unsupported model format: " + saved.assetPath);
                }
            }
        }

        if (!model) {
            appendLog("Snapshot restore warning: failed to load model for entity: " + saved.name);
            continue;
        }

        const SceneEntityId id = m_scene.createEntity(saved.name, model);
        SceneEntity* restored = m_scene.findEntity(id);
        if (!restored) {
            continue;
        }
        restored->sourceAssetPath = saved.assetPath;
        restored->script = SceneEntity::ScriptComponent{};
        restored->script->enabled = saved.scriptEnabled;
        restored->script->scriptPath = saved.scriptPath;
        restored->physics = SceneEntity::PhysicsComponent{};
        restored->physics->enabled = saved.physicsEnabled;
        restored->physics->dynamic = saved.physicsDynamic;
        restored->physics->halfExtents = saved.physicsHalfExtents;
        restored->transform = saved.transform;
        restored->tint = saved.tint;
        if (saved.hasLight) {
            restored->light = SceneEntity::LightComponent{};
            restored->light->type = static_cast<SceneEntity::LightComponent::Type>(saved.lightType);
            restored->light->enabled = saved.lightEnabled;
            restored->light->color = saved.lightColor;
            restored->light->intensity = saved.lightIntensity;
            restored->light->range = saved.lightRange;
        }
        if (saved.hasParticleEmitter) {
            restored->particleEmitter = SceneEntity::ParticleEmitterComponent{};
            auto& pe = *restored->particleEmitter;
            pe.enabled = saved.particleEmitterEnabled;
            pe.preset = static_cast<SceneEntity::ParticleEmitterComponent::Preset>(saved.particleEmitterPreset);
            pe.blendMode = static_cast<SceneEntity::ParticleEmitterComponent::BlendMode>(saved.particleEmitterBlendMode);
            pe.maxParticles = saved.particleEmitterMaxParticles;
            pe.emissionRate = saved.particleEmitterEmissionRate;
            pe.particleLifetime = saved.particleEmitterLifetime;
            pe.velocity = saved.particleEmitterVelocity;
            pe.velocityVariance = saved.particleEmitterVelocityVariance;
            pe.positionVariance = saved.particleEmitterPositionVariance;
            pe.acceleration = saved.particleEmitterAcceleration;
            pe.damping = saved.particleEmitterDamping;
            pe.looping = saved.particleEmitterLooping;
            pe.burstCount = saved.particleEmitterBurstCount;
            pe.burstInterval = saved.particleEmitterBurstInterval;
            pe.burstOnStart = saved.particleEmitterBurstOnStart;
            pe.colorStart = saved.particleEmitterColorStart;
            pe.colorMid = saved.particleEmitterColorMid;
            pe.colorEnd = saved.particleEmitterColorEnd;
            pe.colorMidPoint = saved.particleEmitterColorMidPoint;
            pe.sizeStart = saved.particleEmitterSizeStart;
            pe.sizeMid = saved.particleEmitterSizeMid;
            pe.sizeEnd = saved.particleEmitterSizeEnd;
            pe.sizeMidPoint = saved.particleEmitterSizeMidPoint;
            pe.spriteColumns = saved.particleEmitterSpriteColumns;
            pe.spriteRows = saved.particleEmitterSpriteRows;
            pe.spriteFrameRate = saved.particleEmitterSpriteFrameRate;
            pe.randomStartFrame = saved.particleEmitterRandomStartFrame;
            pe.spriteTexturePath = saved.particleEmitterSpriteTexturePath;
            pe.collideWithGround = saved.particleEmitterCollideWithGround;
            pe.collisionPlaneNormal = saved.particleEmitterCollisionPlaneNormal;
            pe.collisionPlaneOffset = saved.particleEmitterCollisionPlaneOffset;
            pe.bounceFactor = saved.particleEmitterBounceFactor;
            pe.collisionFade = saved.particleEmitterCollisionFade;
            pe.maxBounces = saved.particleEmitterMaxBounces;
        }
        if (saved.hasAudio) {
            restored->audio = SceneEntity::AudioComponent{};
            restored->audio->enabled = saved.audioEnabled;
            restored->audio->clipPath = saved.audioClipPath;
            restored->audio->gain = saved.audioGain;
            restored->audio->minDistance = saved.audioMinDistance;
            restored->audio->maxDistance = saved.audioMaxDistance;
            restored->audio->looping = saved.audioLooping;
        }
    }

    m_walkmeshDirty = true;
    rebuildWalkmesh();
    if (m_walkmeshDebugEnabled) {
        m_viewport.uploadWalkmeshLines(
            m_tileWalkmesh.buildWalkableLines(),
            m_tileWalkmesh.buildBlockedLines());
    }

    m_suppressHistory = false;
}

void Application::captureUndoSnapshot(const std::string& reason) {
    if (m_suppressHistory) {
        return;
    }

    const EditorStateSnapshot snapshot = makeSnapshot();
    if (!m_undoStack.empty() && snapshotsEqual(m_undoStack.back(), snapshot)) {
        return;
    }

    m_undoStack.push_back(snapshot);
    if (m_undoStack.size() > m_maxHistoryEntries) {
        m_undoStack.erase(m_undoStack.begin());
    }
    m_redoStack.clear();

    if (!reason.empty()) {
        appendLog("History checkpoint: " + reason);
    }
}

void Application::clearHistory() {
    m_undoStack.clear();
    m_redoStack.clear();
}

bool Application::undo() {
    if (m_undoStack.empty()) {
        appendLog("Undo: no history.");
        return false;
    }

    const EditorStateSnapshot current = makeSnapshot();
    if (m_redoStack.empty() || !snapshotsEqual(m_redoStack.back(), current)) {
        m_redoStack.push_back(current);
    }

    const EditorStateSnapshot target = m_undoStack.back();
    if (m_undoStack.size() > 1U) {
        m_undoStack.pop_back();
    }

    applySnapshot(target);
    appendLog("Undo");
    return true;
}

bool Application::redo() {
    if (m_redoStack.empty()) {
        appendLog("Redo: no history.");
        return false;
    }

    const EditorStateSnapshot current = makeSnapshot();
    if (m_undoStack.empty() || !snapshotsEqual(m_undoStack.back(), current)) {
        m_undoStack.push_back(current);
        if (m_undoStack.size() > m_maxHistoryEntries) {
            m_undoStack.erase(m_undoStack.begin());
        }
    }

    const EditorStateSnapshot target = m_redoStack.back();
    m_redoStack.pop_back();
    applySnapshot(target);
    appendLog("Redo");
    return true;
}

bool Application::saveAreaToFile(const std::filesystem::path& filePath) {
    const EditorStateSnapshot snapshot = makeSnapshot();

    json root;
    root["format"] = "elysium-area";
    root["version"] = 1;
    root["assetRoot"] = m_assetCatalog.root().lexically_normal().string();

    root["editor"] = {
        {"tileSize", snapshot.tileSize},
        {"gridEnabled", snapshot.gridEnabled},
        {"snapNewPlacements", snapshot.snapNewPlacements},
        {"tileBrushEnabled", snapshot.tileBrushEnabled},
        {"brushWalkable", snapshot.brushWalkable},
        {"tileBrushTint", vec4ToJson(snapshot.tileBrushTint)}
    };

    root["tiles"] = json::array();
    for (const auto& tile : snapshot.tiles) {
        root["tiles"].push_back({
            {"x", tile.x},
            {"z", tile.z},
            {"tint", vec4ToJson(tile.data.tint)},
            {"walkable", tile.data.walkable}
        });
    }

    root["entities"] = json::array();
    for (const auto& entity : snapshot.entities) {
        std::filesystem::path assetPath{entity.assetPath};
        if (assetPath.is_absolute()) {
            const std::filesystem::path rel = assetPath.lexically_relative(m_assetCatalog.root());
            if (!rel.empty() && rel.native().find("..") != 0) {
                assetPath = rel;
            }
        }

        std::filesystem::path spriteTexturePath{entity.particleEmitterSpriteTexturePath};
        if (!spriteTexturePath.empty() && spriteTexturePath.is_absolute()) {
            const std::filesystem::path rel = spriteTexturePath.lexically_relative(m_assetCatalog.root());
            if (!rel.empty() && rel.native().find("..") != 0) {
                spriteTexturePath = rel;
            }
        }

        std::filesystem::path audioClipPath{entity.audioClipPath};
        if (!audioClipPath.empty() && audioClipPath.is_absolute()) {
            const std::filesystem::path rel = audioClipPath.lexically_relative(m_assetCatalog.root());
            if (!rel.empty() && rel.native().find("..") != 0) {
                audioClipPath = rel;
            }
        }

        root["entities"].push_back({
            {"name", entity.name},
            {"asset", assetPath.generic_string()},
            {"scriptEnabled", entity.scriptEnabled},
            {"scriptPath", entity.scriptPath},
            {"physicsEnabled", entity.physicsEnabled},
            {"physicsDynamic", entity.physicsDynamic},
            {"physicsHalfExtents", vec3ToJson(entity.physicsHalfExtents)},
            {"hasLight", entity.hasLight},
            {"lightType", entity.lightType},
            {"lightEnabled", entity.lightEnabled},
            {"lightColor", vec3ToJson(entity.lightColor)},
            {"lightIntensity", entity.lightIntensity},
            {"lightRange", entity.lightRange},
            {"hasParticleEmitter", entity.hasParticleEmitter},
            {"particleEmitterEnabled", entity.particleEmitterEnabled},
            {"particleEmitterPreset", entity.particleEmitterPreset},
            {"particleEmitterBlendMode", entity.particleEmitterBlendMode},
            {"particleEmitterMaxParticles", entity.particleEmitterMaxParticles},
            {"particleEmitterEmissionRate", entity.particleEmitterEmissionRate},
            {"particleEmitterLifetime", entity.particleEmitterLifetime},
            {"particleEmitterVelocity", vec3ToJson(entity.particleEmitterVelocity)},
            {"particleEmitterVelocityVariance", vec3ToJson(entity.particleEmitterVelocityVariance)},
            {"particleEmitterPositionVariance", vec3ToJson(entity.particleEmitterPositionVariance)},
            {"particleEmitterAcceleration", vec3ToJson(entity.particleEmitterAcceleration)},
            {"particleEmitterDamping", entity.particleEmitterDamping},
            {"particleEmitterLooping", entity.particleEmitterLooping},
            {"particleEmitterBurstCount", entity.particleEmitterBurstCount},
            {"particleEmitterBurstInterval", entity.particleEmitterBurstInterval},
            {"particleEmitterBurstOnStart", entity.particleEmitterBurstOnStart},
            {"particleEmitterColorStart", vec4ToJson(entity.particleEmitterColorStart)},
            {"particleEmitterColorMid", vec4ToJson(entity.particleEmitterColorMid)},
            {"particleEmitterColorEnd", vec4ToJson(entity.particleEmitterColorEnd)},
            {"particleEmitterColorMidPoint", entity.particleEmitterColorMidPoint},
            {"particleEmitterSizeStart", entity.particleEmitterSizeStart},
            {"particleEmitterSizeMid", entity.particleEmitterSizeMid},
            {"particleEmitterSizeEnd", entity.particleEmitterSizeEnd},
            {"particleEmitterSizeMidPoint", entity.particleEmitterSizeMidPoint},
            {"particleEmitterSpriteColumns", entity.particleEmitterSpriteColumns},
            {"particleEmitterSpriteRows", entity.particleEmitterSpriteRows},
            {"particleEmitterSpriteFrameRate", entity.particleEmitterSpriteFrameRate},
            {"particleEmitterRandomStartFrame", entity.particleEmitterRandomStartFrame},
            {"particleEmitterSpriteTexturePath", spriteTexturePath.generic_string()},
            {"particleEmitterCollideWithGround", entity.particleEmitterCollideWithGround},
            {"particleEmitterCollisionPlaneNormal", vec3ToJson(entity.particleEmitterCollisionPlaneNormal)},
            {"particleEmitterCollisionPlaneOffset", entity.particleEmitterCollisionPlaneOffset},
            {"particleEmitterBounceFactor", entity.particleEmitterBounceFactor},
            {"particleEmitterCollisionFade", entity.particleEmitterCollisionFade},
            {"particleEmitterMaxBounces", entity.particleEmitterMaxBounces},
            {"hasAudio", entity.hasAudio},
            {"audioEnabled", entity.audioEnabled},
            {"audioClipPath", audioClipPath.generic_string()},
            {"audioGain", entity.audioGain},
            {"audioMinDistance", entity.audioMinDistance},
            {"audioMaxDistance", entity.audioMaxDistance},
            {"audioLooping", entity.audioLooping},
            {"position", vec3ToJson(entity.transform.position)},
            {"rotationDeg", vec3ToJson(entity.transform.rotationEulerDeg)},
            {"scale", vec3ToJson(entity.transform.scale)},
            {"tint", vec4ToJson(entity.tint)}
        });
    }

    std::ofstream out(filePath);
    if (!out.is_open()) {
        appendLog("Save failed: cannot open file: " + filePath.string());
        return false;
    }

    out << root.dump(2);
    if (!out.good()) {
        appendLog("Save failed: write error: " + filePath.string());
        return false;
    }

    m_currentAreaPath = filePath;
    m_areaPathInput.fill('\0');
    const std::string pathString = m_currentAreaPath.string();
    std::strncpy(m_areaPathInput.data(), pathString.c_str(), m_areaPathInput.size() - 1U);

    appendLog("Saved area: " + filePath.string());
    return true;
}

bool Application::loadAreaFromFile(const std::filesystem::path& filePath) {
    std::ifstream in(filePath);
    if (!in.is_open()) {
        appendLog("Load failed: cannot open file: " + filePath.string());
        return false;
    }

    std::stringstream buffer;
    buffer << in.rdbuf();

    json root;
    try {
        root = json::parse(buffer.str());
    } catch (const std::exception& ex) {
        appendLog(std::string("Load failed: invalid JSON: ") + ex.what());
        return false;
    }

    if (!root.contains("format") || root["format"].get<std::string>() != "elysium-area") {
        appendLog("Load failed: unsupported area format.");
        return false;
    }

    EditorStateSnapshot snapshot{};
    snapshot.tileSize = root.value("editor", json::object()).value("tileSize", m_tileMap.tileSize());
    snapshot.gridEnabled = root.value("editor", json::object()).value("gridEnabled", true);
    snapshot.snapNewPlacements = root.value("editor", json::object()).value("snapNewPlacements", true);
    snapshot.tileBrushEnabled = root.value("editor", json::object()).value("tileBrushEnabled", false);
    snapshot.brushWalkable = root.value("editor", json::object()).value("brushWalkable", true);
    jsonToVec4(root.value("editor", json::object()).value("tileBrushTint", json::array({0.6f, 0.68f, 0.74f, 1.0f})), snapshot.tileBrushTint);

    const std::filesystem::path serializedRoot = std::filesystem::path(root.value("assetRoot", std::string{}));

    if (root.contains("tiles") && root["tiles"].is_array()) {
        for (const auto& item : root["tiles"]) {
            SavedTile tile{};
            tile.x = item.value("x", 0);
            tile.z = item.value("z", 0);
            jsonToVec4(item.value("tint", json::array({0.6f, 0.68f, 0.74f, 1.0f})), tile.data.tint);
            tile.data.walkable = item.value("walkable", true);
            snapshot.tiles.push_back(tile);
        }
    }

    if (root.contains("entities") && root["entities"].is_array()) {
        for (const auto& item : root["entities"]) {
            SavedEntity entity{};
            entity.name = item.value("name", std::string("Entity"));
            entity.assetPath = item.value("asset", std::string{});
            entity.scriptEnabled = item.value("scriptEnabled", false);
            entity.scriptPath = item.value("scriptPath", std::string{});
            entity.physicsEnabled = item.value("physicsEnabled", false);
            entity.physicsDynamic = item.value("physicsDynamic", true);
            jsonToVec3(item.value("physicsHalfExtents", json::array({0.5f, 0.5f, 0.5f})), entity.physicsHalfExtents);
            entity.hasLight = item.value("hasLight", false);
            entity.lightType = item.value("lightType", 0);
            entity.lightEnabled = item.value("lightEnabled", true);
            jsonToVec3(item.value("lightColor", json::array({1.0f, 1.0f, 1.0f})), entity.lightColor);
            entity.lightIntensity = item.value("lightIntensity", 1.0f);
            entity.lightRange = item.value("lightRange", 10.0f);
            entity.hasParticleEmitter = item.value("hasParticleEmitter", false);
            entity.particleEmitterEnabled = item.value("particleEmitterEnabled", true);
            entity.particleEmitterPreset = item.value("particleEmitterPreset", 0);
            entity.particleEmitterBlendMode = item.value("particleEmitterBlendMode", 0);
            entity.particleEmitterMaxParticles = item.value("particleEmitterMaxParticles", 10000u);
            entity.particleEmitterEmissionRate = item.value("particleEmitterEmissionRate", 100.0f);
            entity.particleEmitterLifetime = item.value("particleEmitterLifetime", 2.0f);
            jsonToVec3(item.value("particleEmitterVelocity", json::array({0.0f, 1.0f, 0.0f})), entity.particleEmitterVelocity);
            jsonToVec3(item.value("particleEmitterVelocityVariance", json::array({0.5f, 0.2f, 0.5f})), entity.particleEmitterVelocityVariance);
            jsonToVec3(item.value("particleEmitterPositionVariance", json::array({0.1f, 0.1f, 0.1f})), entity.particleEmitterPositionVariance);
            jsonToVec3(item.value("particleEmitterAcceleration", json::array({0.0f, -0.98f, 0.0f})), entity.particleEmitterAcceleration);
            entity.particleEmitterDamping = item.value("particleEmitterDamping", 0.99f);
            entity.particleEmitterLooping = item.value("particleEmitterLooping", true);
            entity.particleEmitterBurstCount = item.value("particleEmitterBurstCount", 0u);
            entity.particleEmitterBurstInterval = item.value("particleEmitterBurstInterval", 1.0f);
            entity.particleEmitterBurstOnStart = item.value("particleEmitterBurstOnStart", false);
            jsonToVec4(item.value("particleEmitterColorStart", json::array({1.0f, 1.0f, 1.0f, 1.0f})), entity.particleEmitterColorStart);
            jsonToVec4(item.value("particleEmitterColorMid", json::array({1.0f, 1.0f, 1.0f, 0.65f})), entity.particleEmitterColorMid);
            jsonToVec4(item.value("particleEmitterColorEnd", json::array({1.0f, 1.0f, 1.0f, 0.0f})), entity.particleEmitterColorEnd);
            entity.particleEmitterColorMidPoint = item.value("particleEmitterColorMidPoint", 0.45f);
            entity.particleEmitterSizeStart = item.value("particleEmitterSizeStart", 0.1f);
            entity.particleEmitterSizeMid = item.value("particleEmitterSizeMid", 0.08f);
            entity.particleEmitterSizeEnd = item.value("particleEmitterSizeEnd", 0.05f);
            entity.particleEmitterSizeMidPoint = item.value("particleEmitterSizeMidPoint", 0.45f);
            entity.particleEmitterSpriteColumns = item.value("particleEmitterSpriteColumns", 1);
            entity.particleEmitterSpriteRows = item.value("particleEmitterSpriteRows", 1);
            entity.particleEmitterSpriteFrameRate = item.value("particleEmitterSpriteFrameRate", 0.0f);
            entity.particleEmitterRandomStartFrame = item.value("particleEmitterRandomStartFrame", false);
            entity.particleEmitterSpriteTexturePath = item.value("particleEmitterSpriteTexturePath", std::string{});
            entity.particleEmitterCollideWithGround = item.value("particleEmitterCollideWithGround", false);
            jsonToVec3(item.value("particleEmitterCollisionPlaneNormal", json::array({0.0f, 1.0f, 0.0f})), entity.particleEmitterCollisionPlaneNormal);
            entity.particleEmitterCollisionPlaneOffset = item.value("particleEmitterCollisionPlaneOffset", 0.0f);
            entity.particleEmitterBounceFactor = item.value("particleEmitterBounceFactor", 0.35f);
            entity.particleEmitterCollisionFade = item.value("particleEmitterCollisionFade", 0.45f);
            entity.particleEmitterMaxBounces = item.value("particleEmitterMaxBounces", 1);
            entity.hasAudio = item.value("hasAudio", false);
            entity.audioEnabled = item.value("audioEnabled", true);
            entity.audioClipPath = item.value("audioClipPath", std::string{});
            entity.audioGain = item.value("audioGain", 1.0f);
            entity.audioMinDistance = item.value("audioMinDistance", 1.0f);
            entity.audioMaxDistance = item.value("audioMaxDistance", 40.0f);
            entity.audioLooping = item.value("audioLooping", false);
            jsonToVec3(item.value("position", json::array({0.0f, 0.0f, 0.0f})), entity.transform.position);
            jsonToVec3(item.value("rotationDeg", json::array({0.0f, 0.0f, 0.0f})), entity.transform.rotationEulerDeg);
            jsonToVec3(item.value("scale", json::array({1.0f, 1.0f, 1.0f})), entity.transform.scale);
            jsonToVec4(item.value("tint", json::array({1.0f, 1.0f, 1.0f, 1.0f})), entity.tint);

            if (!entity.assetPath.empty()) {
                std::filesystem::path assetPath{entity.assetPath};
                if (assetPath.is_relative()) {
                    if (!serializedRoot.empty()) {
                        assetPath = serializedRoot / assetPath;
                    } else {
                        assetPath = std::filesystem::current_path() / assetPath;
                    }
                }
                entity.assetPath = assetPath.lexically_normal().string();
            }

            if (!entity.particleEmitterSpriteTexturePath.empty()) {
                std::filesystem::path texturePath{entity.particleEmitterSpriteTexturePath};
                if (texturePath.is_relative()) {
                    if (!serializedRoot.empty()) {
                        texturePath = serializedRoot / texturePath;
                    } else {
                        texturePath = std::filesystem::current_path() / texturePath;
                    }
                }
                entity.particleEmitterSpriteTexturePath = texturePath.lexically_normal().string();
            }

            if (!entity.audioClipPath.empty()) {
                std::filesystem::path clipPath{entity.audioClipPath};
                if (clipPath.is_relative()) {
                    if (!serializedRoot.empty()) {
                        clipPath = serializedRoot / clipPath;
                    } else {
                        clipPath = std::filesystem::current_path() / clipPath;
                    }
                }
                entity.audioClipPath = clipPath.lexically_normal().string();
            }

            snapshot.entities.push_back(std::move(entity));
        }
    }

    captureUndoSnapshot("Load area");
    applySnapshot(snapshot);

    m_currentAreaPath = filePath;
    m_areaPathInput.fill('\0');
    const std::string pathString = m_currentAreaPath.string();
    std::strncpy(m_areaPathInput.data(), pathString.c_str(), m_areaPathInput.size() - 1U);

    clearHistory();
    m_undoStack.push_back(makeSnapshot());

    appendLog("Loaded area: " + filePath.string());
    return true;
}

void Application::newArea() {
    captureUndoSnapshot("New area");

    EditorStateSnapshot empty{};
    empty.tileSize = 1.0f;
    empty.gridEnabled = true;
    empty.snapNewPlacements = true;
    empty.tileBrushEnabled = false;
    empty.brushWalkable = true;
    empty.tileBrushTint = glm::vec4(0.60f, 0.68f, 0.74f, 1.0f);

    applySnapshot(empty);
    appendLog("Created new empty area.");
}

void Application::syncParticleEmitters() {
    std::vector<SceneEntityId> staleIds;

    for (const auto& [entityId, emitter] : m_particleEmitters) {
        bool found = false;
        for (const auto& entity : m_scene.entities()) {
            if (entity.id == entityId) {
                found = true;
                break;
            }
        }
        if (!found) {
            staleIds.push_back(entityId);
        }
    }

    for (SceneEntityId entityId : staleIds) {
        auto it = m_particleEmitters.find(entityId);
        if (it != m_particleEmitters.end()) {
            if (it->second) {
                ParticleSystem::instance().destroyEmitter(it->second);
            }
            m_particleEmitters.erase(it);
        }
    }

    for (const auto& entity : m_scene.entities()) {
        const bool wantsEmitter = entity.particleEmitter.has_value() && entity.particleEmitter->enabled;
        auto existing = m_particleEmitters.find(entity.id);

        if (!wantsEmitter) {
            if (existing != m_particleEmitters.end()) {
                if (existing->second) {
                    ParticleSystem::instance().destroyEmitter(existing->second);
                }
                m_particleEmitters.erase(existing);
            }
            continue;
        }

        ParticleEmitterConfig config{};
        config.position = entity.transform.position;
        config.positionVariance = entity.particleEmitter->positionVariance;
        config.velocity = entity.particleEmitter->velocity;
        config.velocityVariance = entity.particleEmitter->velocityVariance;
        config.acceleration = entity.particleEmitter->acceleration;
        config.particleLifetime = entity.particleEmitter->particleLifetime;
        config.emissionRate = entity.particleEmitter->emissionRate;
        config.looping = entity.particleEmitter->looping;
        config.burstCount = entity.particleEmitter->burstCount;
        config.burstInterval = entity.particleEmitter->burstInterval;
        config.burstOnStart = entity.particleEmitter->burstOnStart;
        config.blendMode = static_cast<ParticleEmitterConfig::BlendMode>(entity.particleEmitter->blendMode);
        config.colorStart = entity.particleEmitter->colorStart;
        config.colorMid = entity.particleEmitter->colorMid;
        config.colorEnd = entity.particleEmitter->colorEnd;
        config.colorMidPoint = entity.particleEmitter->colorMidPoint;
        config.sizeStart = entity.particleEmitter->sizeStart;
        config.sizeMid = entity.particleEmitter->sizeMid;
        config.sizeEnd = entity.particleEmitter->sizeEnd;
        config.sizeMidPoint = entity.particleEmitter->sizeMidPoint;
        config.spriteColumns = entity.particleEmitter->spriteColumns;
        config.spriteRows = entity.particleEmitter->spriteRows;
        config.spriteFrameRate = entity.particleEmitter->spriteFrameRate;
        config.randomStartFrame = entity.particleEmitter->randomStartFrame;
        config.spriteTexturePath = entity.particleEmitter->spriteTexturePath;
        config.collideWithGround = entity.particleEmitter->collideWithGround;
        const float normalLengthSq = glm::dot(entity.particleEmitter->collisionPlaneNormal, entity.particleEmitter->collisionPlaneNormal);
        if (normalLengthSq > 1e-4f) {
            config.collisionPlaneNormal = glm::normalize(entity.particleEmitter->collisionPlaneNormal);
        } else {
            config.collisionPlaneNormal = glm::vec3(0.0f, 1.0f, 0.0f);
        }
        config.collisionPlaneOffset = entity.particleEmitter->collisionPlaneOffset;
        config.bounceFactor = entity.particleEmitter->bounceFactor;
        config.collisionFade = entity.particleEmitter->collisionFade;
        config.maxBounces = entity.particleEmitter->maxBounces;
        config.damping = entity.particleEmitter->damping;
        config.enabled = entity.particleEmitter->enabled;

        const bool needsRecreate = existing != m_particleEmitters.end() &&
                                   existing->second &&
                                   existing->second->maxParticleCount() != entity.particleEmitter->maxParticles;

        if (needsRecreate) {
            ParticleSystem::instance().destroyEmitter(existing->second);
            m_particleEmitters.erase(existing);
            existing = m_particleEmitters.end();
        }

        if (existing == m_particleEmitters.end()) {
            m_particleEmitters.emplace(
                entity.id,
                ParticleSystem::instance().createEmitter(config, entity.particleEmitter->maxParticles));
        } else if (existing->second) {
            existing->second->setConfig(config);
        }
    }
}

void Application::syncSpatialAudio() {
    if (m_spatialAudioSubsystem == nullptr) {
        return;
    }

    AudioListenerState listener{};
    listener.position = m_viewport.cameraPosition();
    listener.forward = m_viewport.cameraForward();
    listener.up = m_viewport.cameraUp();
    m_spatialAudioSubsystem->setListener(listener);
    m_spatialAudioSubsystem->clearEmitters();

    for (const auto& entity : m_scene.entities()) {
        if (entity.audio.has_value() && entity.audio->enabled && !entity.audio->clipPath.empty()) {
            AudioEmitterState emitter{};
            emitter.sourceId = "entity://" + std::to_string(static_cast<unsigned long long>(entity.id)) + "/audio";
            emitter.clipPath = entity.audio->clipPath;
            emitter.position = entity.transform.position;
            emitter.gain = std::max(0.0f, entity.audio->gain);
            emitter.minDistance = std::max(0.01f, entity.audio->minDistance);
            emitter.maxDistance = std::max(emitter.minDistance + 0.01f, entity.audio->maxDistance);
            emitter.looping = entity.audio->looping;
            m_spatialAudioSubsystem->submitEmitter(emitter);
            continue;
        }

        if (entity.light.has_value() && entity.light->enabled &&
            entity.light->type == SceneEntity::LightComponent::Type::Point) {
            AudioEmitterState emitter{};
            emitter.sourceId = "entity://" + std::to_string(static_cast<unsigned long long>(entity.id)) + "/light";
            emitter.clipPath = "light://point";
            emitter.position = entity.transform.position;
            emitter.gain = std::clamp(entity.light->intensity * 0.1f, 0.0f, 1.0f);
            emitter.minDistance = 1.5f;
            emitter.maxDistance = std::max(2.0f, entity.light->range);
            emitter.looping = true;
            m_spatialAudioSubsystem->submitEmitter(emitter);
        }

        if (entity.particleEmitter.has_value() && entity.particleEmitter->enabled) {
            const auto& pe = *entity.particleEmitter;
            if (pe.emissionRate <= 0.0f && pe.burstCount == 0U) {
                continue;
            }

            AudioEmitterState emitter{};
            emitter.sourceId = "entity://" + std::to_string(static_cast<unsigned long long>(entity.id)) + "/particles";
            emitter.clipPath = "particles://" + std::string(particlePresetLabel(pe.preset));
            emitter.position = entity.transform.position;
            emitter.gain = std::clamp((pe.emissionRate / 220.0f) + (static_cast<float>(pe.burstCount) / 64.0f), 0.05f, 1.0f);
            emitter.minDistance = 1.0f;
            emitter.maxDistance = 18.0f;
            emitter.looping = pe.looping;
            m_spatialAudioSubsystem->submitEmitter(emitter);
        }
    }
}

void Application::rebuildWalkmesh() {
    m_tileWalkmesh = m_tileMap.buildWalkmesh();
    if (m_physicsRuntimeSubsystem != nullptr) {
        m_physicsRuntimeSubsystem->setTilesDirty(true);
    }
    m_walkmeshDirty = false;

    if (m_walkmeshDebugEnabled) {
        m_viewport.uploadWalkmeshLines(
            m_tileWalkmesh.buildWalkableLines(),
            m_tileWalkmesh.buildBlockedLines());
    }
    if (m_physicsDebugEnabled) {
        m_viewport.uploadPhysicsLines(m_physicsSystem.buildDebugLines());
    }

    appendLog("Walkmesh rebuilt: " + std::to_string(m_tileWalkmesh.triangleCount()) + " triangles.");
}

void Application::drawCharacterPreview(const ImVec2& imageMin, const ImVec2& imageMax) {
    // Project foot + half-height to screen for the avatar circle centre.
    const glm::vec3 avatarWorld = m_controller.footPosition() + glm::vec3{0.0f, 0.5f, 0.0f};
    ImVec2 avatarScreen;
    if (!m_viewport.worldToScreen(avatarWorld, imageMin, imageMax, avatarScreen)) {
        return; // character is off-screen or behind the camera
    }

    ImDrawList* dl = ImGui::GetWindowDrawList();

    // Body: filled teal circle with white outline.
    dl->AddCircleFilled(avatarScreen, 9.0f, IM_COL32(50, 195, 220, 210));
    dl->AddCircle(avatarScreen, 9.0f, IM_COL32(255, 255, 255, 240), 16, 1.5f);

    // Direction arrow: project a world point 1.4 m in front of the character.
    const float facingRad   = m_controller.facingYawDeg() * 3.14159265f / 180.0f;
    const glm::vec3 fwd3d   = {-std::sin(facingRad), 0.0f, -std::cos(facingRad)};
    const glm::vec3 tipWorld = m_controller.footPosition() + glm::vec3{0.0f, 0.5f, 0.0f} + fwd3d * 1.4f;
    ImVec2 tipScreen;
    if (m_viewport.worldToScreen(tipWorld, imageMin, imageMax, tipScreen)) {
        dl->AddLine(avatarScreen, tipScreen, IM_COL32(255, 240, 60, 255), 2.5f);
        dl->AddCircleFilled(tipScreen, 3.5f, IM_COL32(255, 240, 60, 255));
    }

    // Label with foot position.
    const glm::vec3 fp = m_controller.footPosition();
    char label[64];
    std::snprintf(label, sizeof(label), "[Preview]  %.1f, %.2f, %.1f", fp.x, fp.y, fp.z);
    dl->AddText(ImVec2(avatarScreen.x + 13.0f, avatarScreen.y - 9.0f),
                IM_COL32(180, 230, 255, 255), label);
}

} // namespace elysium
