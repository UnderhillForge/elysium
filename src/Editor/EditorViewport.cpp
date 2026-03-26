#include "Editor/EditorViewport.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "Particles/ParticleSystem.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <spdlog/spdlog.h>

#include <array>
#include <cmath>
#include <vector>
#include <string>

namespace elysium {

bool EditorViewport::initialize() {
    if (!buildShaders()) {
        return false;
    }
    if (!buildSkyShader()) {
        return false;
    }
    if (!buildGridShader()) {
        return false;
    }
    if (!buildTileShader()) {
        return false;
    }
    buildGridGeometry();
    buildTileGeometry();
    glCreateVertexArrays(1, &m_skyVao);
    recreateFramebuffer();
    return true;
}

void EditorViewport::shutdown() {
    if (m_physicsVbo) glDeleteBuffers(1, &m_physicsVbo);
    if (m_physicsVao) glDeleteVertexArrays(1, &m_physicsVao);
    if (m_skyVao) glDeleteVertexArrays(1, &m_skyVao);
    m_skyVao = 0;
    m_physicsVao = 0;
    m_physicsVbo = 0;
    m_physicsLineVertexCount = 0;
    if (m_walkmeshVbo) glDeleteBuffers(1, &m_walkmeshVbo);
    if (m_walkmeshVao) glDeleteVertexArrays(1, &m_walkmeshVao);
    m_walkmeshVao = 0;
    m_walkmeshVbo = 0;
    m_walkmeshWalkableCount = 0;
    m_walkmeshBlockedOffset = 0;
    m_walkmeshBlockedCount  = 0;
    if (m_tileInstanceVbo) glDeleteBuffers(1, &m_tileInstanceVbo);
    if (m_tileIbo) glDeleteBuffers(1, &m_tileIbo);
    if (m_tileVbo) glDeleteBuffers(1, &m_tileVbo);
    if (m_tileVao) glDeleteVertexArrays(1, &m_tileVao);
    if (m_gridVbo) glDeleteBuffers(1, &m_gridVbo);
    if (m_gridVao) glDeleteVertexArrays(1, &m_gridVao);
    if (m_depthTex) glDeleteTextures(1, &m_depthTex);
    if (m_colorTex) glDeleteTextures(1, &m_colorTex);
    if (m_fbo) glDeleteFramebuffers(1, &m_fbo);
    m_tileInstanceVbo = 0;
    m_tileIbo = 0;
    m_tileVbo = 0;
    m_tileVao = 0;
    m_tileIndexCount = 0;
    m_gridVbo = 0;
    m_gridVao = 0;
    m_gridVertexCount = 0;
    m_depthTex = 0;
    m_colorTex = 0;
    m_fbo = 0;
}

void EditorViewport::resize(int width, int height) {
    if (width == m_width && height == m_height) {
        return;
    }
    m_width = (width > 1) ? width : 1;
    m_height = (height > 1) ? height : 1;
    recreateFramebuffer();
}

void EditorViewport::updateCamera(float dt, bool hovered, bool focused) {
    const bool active = hovered && focused;
    m_camera.setAspect(static_cast<float>(m_width) / static_cast<float>(m_height));
    m_camera.updateOrbitPanZoom(dt, active);
    m_camera.updateFly(dt, active);
}

void EditorViewport::render(const Scene& scene, const TileMap& tileMap) {
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    glViewport(0, 0, m_width, m_height);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glClearColor(0.10f, 0.11f, 0.13f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    const glm::mat4 view        = m_camera.viewMatrix();
    const glm::mat4 proj        = m_camera.projectionMatrix();
    const glm::mat4 viewProj    = proj * view;
    const glm::mat4 invViewProj = glm::inverse(viewProj);

    // ---- Collect lights from scene ----------------------------------------
    // Default sun (always active as fallback)
    glm::vec3 sunDir{-0.45f, -1.0f, -0.35f};
    glm::vec3 sunColor{m_sunColor};
    float     sunIntensity{1.0f};

    struct PL { glm::vec3 pos; glm::vec3 color; float intensity; float range; };
    std::array<PL, 4> pointLights{};
    int numPL{0};

    for (const auto& entity : scene.entities()) {
        if (!entity.light || !entity.light->enabled) continue;
        if (entity.light->type == SceneEntity::LightComponent::Type::Directional) {
            const float px = glm::radians(entity.transform.rotationEulerDeg.x);
            const float py = glm::radians(entity.transform.rotationEulerDeg.y);
            sunDir       = glm::normalize(glm::vec3(-std::sin(py)*std::cos(px), -std::sin(px), -std::cos(py)*std::cos(px)));
            sunColor     = entity.light->color;
            sunIntensity = entity.light->intensity;
        } else if (entity.light->type == SceneEntity::LightComponent::Type::Point && numPL < 4) {
            pointLights[numPL++] = {entity.transform.position, entity.light->color, entity.light->intensity, entity.light->range};
        }
    }

    // ---- Sky background ---------------------------------------------------
    if (m_drawSky) {
        glDepthMask(GL_FALSE);
        glDisable(GL_DEPTH_TEST);
        drawSky(invViewProj, sunDir);
        glDepthMask(GL_TRUE);
        glEnable(GL_DEPTH_TEST);
    }

    // ---- Grid / tiles / overlays -----------------------------------------
    if (m_gridEnabled) drawGrid(viewProj);
    drawTiles(viewProj, tileMap);
    if (m_drawWalkmesh && (m_walkmeshWalkableCount + m_walkmeshBlockedCount) > 0) drawWalkmeshDebug(viewProj);
    if (m_drawPhysics && m_physicsLineVertexCount > 0) drawPhysicsDebug(viewProj);

    // ---- Entity shader: set all uniforms then draw scene -----------------
    m_shader.bind();
    glUniformMatrix4fv(glGetUniformLocation(m_shader.id(), "u_ViewProj"),   1, GL_FALSE, glm::value_ptr(viewProj));
    glUniform3fv(glGetUniformLocation(m_shader.id(), "u_CameraPos"),        1, glm::value_ptr(m_camera.position()));
    glUniform3fv(glGetUniformLocation(m_shader.id(), "u_AmbientColor"),     1, glm::value_ptr(m_ambientColor));
    glUniform1i(glGetUniformLocation(m_shader.id(), "u_MaterialDebugView"), static_cast<int>(m_materialDebugView));
    glUniform1i(glGetUniformLocation(m_shader.id(), "u_EnableToneMapping"), m_toneMappingEnabled ? 1 : 0);
    glUniform1f(glGetUniformLocation(m_shader.id(), "u_Exposure"), m_exposure);
    glUniform1f(glGetUniformLocation(m_shader.id(), "u_Gamma"), m_gamma);

    // Directional light (always on with either scene or default direction)
    const glm::vec3 normSunDir = glm::normalize(sunDir);
    glUniform1i(glGetUniformLocation(m_shader.id(), "u_HasDirLight"), 1);
    glUniform3fv(glGetUniformLocation(m_shader.id(), "u_DirLight.direction"),  1, glm::value_ptr(normSunDir));
    glUniform3fv(glGetUniformLocation(m_shader.id(), "u_DirLight.color"),      1, glm::value_ptr(sunColor));
    glUniform1f(glGetUniformLocation(m_shader.id(),  "u_DirLight.intensity"),  sunIntensity);

    // Point lights
    glUniform1i(glGetUniformLocation(m_shader.id(), "u_NumPointLights"), numPL);
    for (int i = 0; i < numPL; ++i) {
        const std::string base = "u_PointLights[" + std::to_string(i) + "].";
        glUniform3fv(glGetUniformLocation(m_shader.id(), (base+"position").c_str()),  1, glm::value_ptr(pointLights[i].pos));
        glUniform3fv(glGetUniformLocation(m_shader.id(), (base+"color").c_str()),     1, glm::value_ptr(pointLights[i].color));
        glUniform1f(glGetUniformLocation(m_shader.id(),  (base+"intensity").c_str()), pointLights[i].intensity);
        glUniform1f(glGetUniformLocation(m_shader.id(),  (base+"range").c_str()),     pointLights[i].range);
    }

    scene.draw(m_shader.id());
        ParticleSystem::instance().render(
            viewProj,
            m_depthTex,
            glm::vec2(static_cast<float>(m_width), static_cast<float>(m_height)),
            m_camera.nearPlane(),
            m_camera.farPlane());
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

bool EditorViewport::screenPointToGround(
    const ImVec2& mousePos,
    const ImVec2& imageMin,
    const ImVec2& imageMax,
    glm::vec3& outGroundPoint
) const {
    const float width = imageMax.x - imageMin.x;
    const float height = imageMax.y - imageMin.y;
    if (width <= 1.0f || height <= 1.0f) {
        return false;
    }

    if (mousePos.x < imageMin.x || mousePos.x > imageMax.x || mousePos.y < imageMin.y || mousePos.y > imageMax.y) {
        return false;
    }

    const float localX = (mousePos.x - imageMin.x) / width;
    const float localY = (mousePos.y - imageMin.y) / height;

    const float ndcX = localX * 2.0f - 1.0f;
    const float ndcY = 1.0f - localY * 2.0f;

    const glm::mat4 viewProj = m_camera.projectionMatrix() * m_camera.viewMatrix();
    const glm::mat4 invViewProj = glm::inverse(viewProj);

    glm::vec4 nearClip{ndcX, ndcY, -1.0f, 1.0f};
    glm::vec4 farClip{ndcX, ndcY, 1.0f, 1.0f};

    glm::vec4 nearWorld4 = invViewProj * nearClip;
    glm::vec4 farWorld4 = invViewProj * farClip;
    if (nearWorld4.w == 0.0f || farWorld4.w == 0.0f) {
        return false;
    }

    const glm::vec3 nearWorld = glm::vec3(nearWorld4) / nearWorld4.w;
    const glm::vec3 farWorld = glm::vec3(farWorld4) / farWorld4.w;
    const glm::vec3 rayDir = glm::normalize(farWorld - nearWorld);

    if (std::abs(rayDir.y) < 0.00001f) {
        return false;
    }

    const float t = -nearWorld.y / rayDir.y;
    if (t < 0.0f) {
        return false;
    }

    outGroundPoint = nearWorld + rayDir * t;
    return true;
}

bool EditorViewport::worldToScreen(
    const glm::vec3& worldPoint,
    const ImVec2& imageMin,
    const ImVec2& imageMax,
    ImVec2& outScreenPoint
) const {
    const glm::mat4 viewProj = m_camera.projectionMatrix() * m_camera.viewMatrix();
    const glm::vec4 clip = viewProj * glm::vec4(worldPoint, 1.0f);
    if (clip.w <= 0.0f) {
        return false;
    }

    const glm::vec3 ndc = glm::vec3(clip) / clip.w;
    if (ndc.z < -1.0f || ndc.z > 1.0f) {
        return false;
    }

    const float width = imageMax.x - imageMin.x;
    const float height = imageMax.y - imageMin.y;
    outScreenPoint.x = imageMin.x + (ndc.x * 0.5f + 0.5f) * width;
    outScreenPoint.y = imageMin.y + (1.0f - (ndc.y * 0.5f + 0.5f)) * height;
    return true;
}

void EditorViewport::uploadWalkmeshLines(const std::vector<glm::vec3>& walkableLines,
                                          const std::vector<glm::vec3>& blockedLines) {
    // Lazily create VAO / VBO on first upload.
    if (m_walkmeshVao == 0) {
        glCreateVertexArrays(1, &m_walkmeshVao);
        glCreateBuffers(1, &m_walkmeshVbo);
        glVertexArrayVertexBuffer(m_walkmeshVao, 0, m_walkmeshVbo, 0, sizeof(glm::vec3));
        glEnableVertexArrayAttrib(m_walkmeshVao, 0);
        glVertexArrayAttribFormat(m_walkmeshVao, 0, 3, GL_FLOAT, GL_FALSE, 0);
        glVertexArrayAttribBinding(m_walkmeshVao, 0, 0);
    }

    // Pack both groups into one buffer: walkable first, then blocked.
    m_walkmeshWalkableCount = static_cast<GLsizei>(walkableLines.size());
    m_walkmeshBlockedOffset = m_walkmeshWalkableCount;
    m_walkmeshBlockedCount  = static_cast<GLsizei>(blockedLines.size());

    const std::size_t totalBytes =
        (walkableLines.size() + blockedLines.size()) * sizeof(glm::vec3);

    if (totalBytes == 0) return;

    glNamedBufferData(m_walkmeshVbo, static_cast<GLsizeiptr>(totalBytes), nullptr, GL_DYNAMIC_DRAW);

    if (!walkableLines.empty()) {
        glNamedBufferSubData(m_walkmeshVbo, 0,
            static_cast<GLsizeiptr>(walkableLines.size() * sizeof(glm::vec3)),
            walkableLines.data());
    }
    if (!blockedLines.empty()) {
        glNamedBufferSubData(m_walkmeshVbo,
            static_cast<GLsizeiptr>(m_walkmeshBlockedOffset * sizeof(glm::vec3)),
            static_cast<GLsizeiptr>(blockedLines.size() * sizeof(glm::vec3)),
            blockedLines.data());
    }
}

void EditorViewport::drawWalkmeshDebug(const glm::mat4& viewProj) {
    // Reuse the grid shader (uniform-colour line renderer).
    m_gridShader.bind();
    glUniformMatrix4fv(glGetUniformLocation(m_gridShader.id(), "u_ViewProj"),
                       1, GL_FALSE, glm::value_ptr(viewProj));

    glBindVertexArray(m_walkmeshVao);

    // Draw walkable triangles in green.
    if (m_walkmeshWalkableCount > 0) {
        glUniform4f(glGetUniformLocation(m_gridShader.id(), "u_Color"), 0.20f, 0.85f, 0.35f, 0.80f);
        glDrawArrays(GL_LINES, 0, m_walkmeshWalkableCount);
    }

    // Draw blocked triangles in red.
    if (m_walkmeshBlockedCount > 0) {
        glUniform4f(glGetUniformLocation(m_gridShader.id(), "u_Color"), 0.90f, 0.22f, 0.22f, 0.80f);
        glDrawArrays(GL_LINES, m_walkmeshBlockedOffset, m_walkmeshBlockedCount);
    }

    glBindVertexArray(0);
}

void EditorViewport::uploadPhysicsLines(const std::vector<glm::vec3>& lines) {
    if (m_physicsVao == 0) {
        glCreateVertexArrays(1, &m_physicsVao);
        glCreateBuffers(1, &m_physicsVbo);
        glVertexArrayVertexBuffer(m_physicsVao, 0, m_physicsVbo, 0, sizeof(glm::vec3));
        glEnableVertexArrayAttrib(m_physicsVao, 0);
        glVertexArrayAttribFormat(m_physicsVao, 0, 3, GL_FLOAT, GL_FALSE, 0);
        glVertexArrayAttribBinding(m_physicsVao, 0, 0);
    }

    m_physicsLineVertexCount = static_cast<GLsizei>(lines.size());
    if (lines.empty()) {
        return;
    }

    glNamedBufferData(
        m_physicsVbo,
        static_cast<GLsizeiptr>(lines.size() * sizeof(glm::vec3)),
        lines.data(),
        GL_DYNAMIC_DRAW
    );
}

void EditorViewport::drawPhysicsDebug(const glm::mat4& viewProj) {
    m_gridShader.bind();
    glUniformMatrix4fv(glGetUniformLocation(m_gridShader.id(), "u_ViewProj"),
                       1, GL_FALSE, glm::value_ptr(viewProj));
    glUniform4f(glGetUniformLocation(m_gridShader.id(), "u_Color"), 0.96f, 0.84f, 0.22f, 0.90f);

    glBindVertexArray(m_physicsVao);
    glDrawArrays(GL_LINES, 0, m_physicsLineVertexCount);
    glBindVertexArray(0);
}

bool EditorViewport::buildGridShader() {
    static const char* kVS = R"(#version 450 core
layout(location = 0) in vec3 aPos;
uniform mat4 u_ViewProj;
void main() {
    gl_Position = u_ViewProj * vec4(aPos, 1.0);
}
)";

    static const char* kFS = R"(#version 450 core
uniform vec4 u_Color;
out vec4 FragColor;
void main() {
    FragColor = u_Color;
}
)";

    return m_gridShader.buildFromSource(kVS, kFS);
}

bool EditorViewport::buildTileShader() {
    static const char* kVS = R"(#version 450 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUV;
layout(location = 3) in vec4 iModel0;
layout(location = 4) in vec4 iModel1;
layout(location = 5) in vec4 iModel2;
layout(location = 6) in vec4 iModel3;
layout(location = 7) in vec4 iTint;

uniform mat4 u_ViewProj;

out vec3 vNormal;
out vec4 vTint;

void main() {
    mat4 model = mat4(iModel0, iModel1, iModel2, iModel3);
    vec4 worldPos = model * vec4(aPos, 1.0);
    vNormal = mat3(transpose(inverse(model))) * aNormal;
    vTint = iTint;
    gl_Position = u_ViewProj * worldPos;
}
)";

    static const char* kFS = R"(#version 450 core
in vec3 vNormal;
in vec4 vTint;

uniform vec3 u_LightDir;

out vec4 FragColor;

void main() {
    vec3 N = normalize(vNormal);
    vec3 L = normalize(-u_LightDir);
    float ndotl = max(dot(N, L), 0.0);
    vec3 lit = vTint.rgb * (0.25 + ndotl * 0.75);
    FragColor = vec4(lit, vTint.a);
}
)";

    return m_tileShader.buildFromSource(kVS, kFS);
}

void EditorViewport::buildGridGeometry() {
    std::vector<glm::vec3> lines;
    constexpr int kHalfExtent = 64;
    lines.reserve(static_cast<std::size_t>((kHalfExtent * 2 + 1) * 4));

    for (int i = -kHalfExtent; i <= kHalfExtent; ++i) {
        const float v = static_cast<float>(i);
        const float e = static_cast<float>(kHalfExtent);

        lines.push_back(glm::vec3(v, 0.0f, -e));
        lines.push_back(glm::vec3(v, 0.0f, e));
        lines.push_back(glm::vec3(-e, 0.0f, v));
        lines.push_back(glm::vec3(e, 0.0f, v));
    }

    m_gridVertexCount = static_cast<GLsizei>(lines.size());

    glCreateVertexArrays(1, &m_gridVao);
    glCreateBuffers(1, &m_gridVbo);
    glNamedBufferData(
        m_gridVbo,
        static_cast<GLsizeiptr>(lines.size() * sizeof(glm::vec3)),
        lines.data(),
        GL_STATIC_DRAW
    );

    glVertexArrayVertexBuffer(m_gridVao, 0, m_gridVbo, 0, sizeof(glm::vec3));
    glEnableVertexArrayAttrib(m_gridVao, 0);
    glVertexArrayAttribFormat(m_gridVao, 0, 3, GL_FLOAT, GL_FALSE, 0);
    glVertexArrayAttribBinding(m_gridVao, 0, 0);
}

void EditorViewport::buildTileGeometry() {
    struct Vertex {
        glm::vec3 pos;
        glm::vec3 nrm;
        glm::vec2 uv;
    };

    // Unit cube centered on origin, scaled per-instance.
    const std::array<Vertex, 24> vertices = {
        Vertex{{-0.5f, -0.5f,  0.5f}, { 0, 0, 1}, {0, 0}}, Vertex{{ 0.5f, -0.5f,  0.5f}, { 0, 0, 1}, {1, 0}},
        Vertex{{ 0.5f,  0.5f,  0.5f}, { 0, 0, 1}, {1, 1}}, Vertex{{-0.5f,  0.5f,  0.5f}, { 0, 0, 1}, {0, 1}},

        Vertex{{ 0.5f, -0.5f, -0.5f}, { 0, 0,-1}, {0, 0}}, Vertex{{-0.5f, -0.5f, -0.5f}, { 0, 0,-1}, {1, 0}},
        Vertex{{-0.5f,  0.5f, -0.5f}, { 0, 0,-1}, {1, 1}}, Vertex{{ 0.5f,  0.5f, -0.5f}, { 0, 0,-1}, {0, 1}},

        Vertex{{-0.5f, -0.5f, -0.5f}, {-1, 0, 0}, {0, 0}}, Vertex{{-0.5f, -0.5f,  0.5f}, {-1, 0, 0}, {1, 0}},
        Vertex{{-0.5f,  0.5f,  0.5f}, {-1, 0, 0}, {1, 1}}, Vertex{{-0.5f,  0.5f, -0.5f}, {-1, 0, 0}, {0, 1}},

        Vertex{{ 0.5f, -0.5f,  0.5f}, { 1, 0, 0}, {0, 0}}, Vertex{{ 0.5f, -0.5f, -0.5f}, { 1, 0, 0}, {1, 0}},
        Vertex{{ 0.5f,  0.5f, -0.5f}, { 1, 0, 0}, {1, 1}}, Vertex{{ 0.5f,  0.5f,  0.5f}, { 1, 0, 0}, {0, 1}},

        Vertex{{-0.5f,  0.5f,  0.5f}, { 0, 1, 0}, {0, 0}}, Vertex{{ 0.5f,  0.5f,  0.5f}, { 0, 1, 0}, {1, 0}},
        Vertex{{ 0.5f,  0.5f, -0.5f}, { 0, 1, 0}, {1, 1}}, Vertex{{-0.5f,  0.5f, -0.5f}, { 0, 1, 0}, {0, 1}},

        Vertex{{-0.5f, -0.5f, -0.5f}, { 0,-1, 0}, {0, 0}}, Vertex{{ 0.5f, -0.5f, -0.5f}, { 0,-1, 0}, {1, 0}},
        Vertex{{ 0.5f, -0.5f,  0.5f}, { 0,-1, 0}, {1, 1}}, Vertex{{-0.5f, -0.5f,  0.5f}, { 0,-1, 0}, {0, 1}},
    };

    const std::array<unsigned int, 36> indices = {
        0, 1, 2, 2, 3, 0,
        4, 5, 6, 6, 7, 4,
        8, 9,10,10,11, 8,
       12,13,14,14,15,12,
       16,17,18,18,19,16,
       20,21,22,22,23,20
    };

    m_tileIndexCount = static_cast<GLsizei>(indices.size());

    glCreateVertexArrays(1, &m_tileVao);
    glCreateBuffers(1, &m_tileVbo);
    glCreateBuffers(1, &m_tileIbo);
    glCreateBuffers(1, &m_tileInstanceVbo);

    glNamedBufferData(m_tileVbo, static_cast<GLsizeiptr>(sizeof(vertices)), vertices.data(), GL_STATIC_DRAW);
    glNamedBufferData(m_tileIbo, static_cast<GLsizeiptr>(sizeof(indices)), indices.data(), GL_STATIC_DRAW);
    glNamedBufferData(m_tileInstanceVbo, static_cast<GLsizeiptr>(sizeof(TileInstanceGPU)), nullptr, GL_DYNAMIC_DRAW);

    glVertexArrayVertexBuffer(m_tileVao, 0, m_tileVbo, 0, sizeof(Vertex));
    glVertexArrayElementBuffer(m_tileVao, m_tileIbo);

    glEnableVertexArrayAttrib(m_tileVao, 0);
    glEnableVertexArrayAttrib(m_tileVao, 1);
    glEnableVertexArrayAttrib(m_tileVao, 2);
    glVertexArrayAttribFormat(m_tileVao, 0, 3, GL_FLOAT, GL_FALSE, offsetof(Vertex, pos));
    glVertexArrayAttribFormat(m_tileVao, 1, 3, GL_FLOAT, GL_FALSE, offsetof(Vertex, nrm));
    glVertexArrayAttribFormat(m_tileVao, 2, 2, GL_FLOAT, GL_FALSE, offsetof(Vertex, uv));
    glVertexArrayAttribBinding(m_tileVao, 0, 0);
    glVertexArrayAttribBinding(m_tileVao, 1, 0);
    glVertexArrayAttribBinding(m_tileVao, 2, 0);

    glVertexArrayVertexBuffer(m_tileVao, 1, m_tileInstanceVbo, 0, sizeof(TileInstanceGPU));

    for (int i = 0; i < 4; ++i) {
        const GLuint attrib = static_cast<GLuint>(3 + i);
        glEnableVertexArrayAttrib(m_tileVao, attrib);
        glVertexArrayAttribFormat(m_tileVao, attrib, 4, GL_FLOAT, GL_FALSE, static_cast<GLuint>(i * sizeof(glm::vec4)));
        glVertexArrayAttribBinding(m_tileVao, attrib, 1);
        glVertexArrayBindingDivisor(m_tileVao, 1, 1);
    }

    glEnableVertexArrayAttrib(m_tileVao, 7);
    glVertexArrayAttribFormat(m_tileVao, 7, 4, GL_FLOAT, GL_FALSE, static_cast<GLuint>(sizeof(glm::mat4)));
    glVertexArrayAttribBinding(m_tileVao, 7, 1);
    glVertexArrayBindingDivisor(m_tileVao, 1, 1);
}

void EditorViewport::drawGrid(const glm::mat4& viewProj) {
    m_gridShader.bind();
    glUniformMatrix4fv(glGetUniformLocation(m_gridShader.id(), "u_ViewProj"), 1, GL_FALSE, glm::value_ptr(viewProj));
    glUniform4f(glGetUniformLocation(m_gridShader.id(), "u_Color"), 0.30f, 0.33f, 0.37f, 1.0f);

    glBindVertexArray(m_gridVao);
    glDrawArrays(GL_LINES, 0, m_gridVertexCount);
    glBindVertexArray(0);
}

void EditorViewport::drawTiles(const glm::mat4& viewProj, const TileMap& tileMap) {
    const auto& instances = tileMap.instanceData();
    if (instances.empty() || m_tileVao == 0 || m_tileIndexCount == 0) {
        return;
    }

    glNamedBufferData(
        m_tileInstanceVbo,
        static_cast<GLsizeiptr>(instances.size() * sizeof(TileInstanceGPU)),
        instances.data(),
        GL_DYNAMIC_DRAW
    );

    m_tileShader.bind();
    glUniformMatrix4fv(glGetUniformLocation(m_tileShader.id(), "u_ViewProj"), 1, GL_FALSE, glm::value_ptr(viewProj));
    glUniform3f(glGetUniformLocation(m_tileShader.id(), "u_LightDir"), -0.45f, -1.0f, -0.35f);

    glBindVertexArray(m_tileVao);
    glDrawElementsInstanced(
        GL_TRIANGLES,
        m_tileIndexCount,
        GL_UNSIGNED_INT,
        nullptr,
        static_cast<GLsizei>(instances.size())
    );
    glBindVertexArray(0);
}

bool EditorViewport::buildShaders() {
    static const char* kVS = R"(#version 450 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUV;
uniform mat4 u_ViewProj;
uniform mat4 u_Model;
out vec3 vWorldPos;
out vec3 vNormal;
out vec2 vUV;
void main() {
    vec4 w   = u_Model * vec4(aPos, 1.0);
    vWorldPos = w.xyz;
    vNormal   = mat3(transpose(inverse(u_Model))) * aNormal;
    vUV       = aUV;
    gl_Position = u_ViewProj * w;
}
)";

    static const char* kFS = R"(#version 450 core
in vec3 vWorldPos;
in vec3 vNormal;
in vec2 vUV;

#define MAX_POINT_LIGHTS 4
struct DirLight   { vec3 direction; vec3 color; float intensity; };
struct PointLight { vec3 position;  vec3 color; float intensity; float range; };

uniform vec3      u_CameraPos;
uniform vec3      u_AmbientColor;
uniform vec4      u_BaseColorFactor;
uniform vec4      u_Tint;

uniform bool      u_HasBaseColorTexture;
uniform sampler2D u_BaseColorTex;
uniform bool      u_HasNormalTexture;
uniform sampler2D u_NormalTex;
uniform bool      u_HasMetallicRoughnessTex;
uniform sampler2D u_MetallicRoughnessTex;
uniform float     u_Roughness;
uniform float     u_Metallic;
uniform bool      u_HasEmissiveTex;
uniform sampler2D u_EmissiveTex;
uniform vec3      u_EmissiveFactor;

uniform int       u_MaterialDebugView;
uniform bool      u_EnableToneMapping;
uniform float     u_Exposure;
uniform float     u_Gamma;

uniform bool      u_HasDirLight;
uniform DirLight  u_DirLight;
uniform int       u_NumPointLights;
uniform PointLight u_PointLights[MAX_POINT_LIGHTS];

out vec4 FragColor;

// Derivative-based TBN normal mapping (no vertex tangents required)
vec3 perturbNormal(vec3 N, vec3 wPos, vec2 uv) {
    vec3 dp1 = dFdx(wPos); vec3 dp2 = dFdy(wPos);
    vec2 du  = dFdx(uv);   vec2 dv  = dFdy(uv);
    vec3 T = normalize(dv.y*dp1 - du.y*dp2);
    vec3 B = normalize(-dv.x*dp1 + du.x*dp2);
    vec3 n = texture(u_NormalTex, uv).xyz * 2.0 - 1.0;
    return normalize(mat3(T, B, N) * n);
}

vec3 blinnPhong(vec3 L, vec3 N, vec3 V, vec3 lCol, float att, vec3 alb, float rough) {
    float NdL = max(dot(N, L), 0.0);
    float sh  = max(2.0 / max(rough*rough, 0.002) - 2.0, 4.0);
    vec3  H   = normalize(L + V);
    float sp  = pow(max(dot(N,H), 0.0), sh) * (1.0 - rough*0.8);
    return (alb*NdL + vec3(sp)*0.25) * lCol * att;
}

vec3 tonemapACESApprox(vec3 x) {
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

void main() {
    vec4 base = u_BaseColorFactor;
    if (u_HasBaseColorTexture) base *= texture(u_BaseColorTex, vUV);
    base *= u_Tint;
    if (base.a < 0.01) discard;

    vec3 N = normalize(vNormal);
    if (u_HasNormalTexture) N = perturbNormal(N, vWorldPos, vUV);

    vec3  V     = normalize(u_CameraPos - vWorldPos);
    float rough = u_Roughness;
    if (u_HasMetallicRoughnessTex) rough *= texture(u_MetallicRoughnessTex, vUV).g;

    vec3 color = u_AmbientColor * base.rgb;

    if (u_HasDirLight) {
        vec3 L = normalize(-u_DirLight.direction);
        color += blinnPhong(L, N, V, u_DirLight.color * u_DirLight.intensity, 1.0, base.rgb, rough);
    }
    for (int i = 0; i < u_NumPointLights && i < MAX_POINT_LIGHTS; ++i) {
        vec3  toL = u_PointLights[i].position - vWorldPos;
        float d   = length(toL);
        vec3  L   = toL / max(d, 0.001);
        float r   = max(u_PointLights[i].range, 0.01);
        float a   = clamp(1.0 - (d/r)*(d/r), 0.0, 1.0); a *= a;
        color += blinnPhong(L, N, V, u_PointLights[i].color * u_PointLights[i].intensity, a, base.rgb, rough);
    }

    vec3 emissive = u_EmissiveFactor;
    if (u_HasEmissiveTex) emissive *= texture(u_EmissiveTex, vUV).rgb;
    color += base.rgb * emissive;

    if (u_MaterialDebugView == 1) {
        color = N * 0.5 + 0.5;
    } else if (u_MaterialDebugView == 2) {
        color = vec3(rough);
    } else if (u_MaterialDebugView == 3) {
        float metal = clamp(u_Metallic, 0.0, 1.0);
        if (u_HasMetallicRoughnessTex) metal *= texture(u_MetallicRoughnessTex, vUV).b;
        color = vec3(metal);
    } else if (u_MaterialDebugView == 4) {
        color = emissive;
    }

    if (u_EnableToneMapping) {
        color = tonemapACESApprox(color * max(u_Exposure, 0.001));
    }

    const float gamma = max(u_Gamma, 0.001);
    color = pow(max(color, vec3(0.0)), vec3(1.0 / gamma));

    FragColor = vec4(color, base.a);
}
)";
    return m_shader.buildFromSource(kVS, kFS);
}

bool EditorViewport::buildSkyShader() {
    static const char* kVS = R"(#version 450 core
out vec4 vClip;
void main() {
    const vec2 pos[3] = vec2[3](vec2(-1.0,-1.0), vec2(3.0,-1.0), vec2(-1.0,3.0));
    vClip = vec4(pos[gl_VertexID], 0.9999, 1.0);
    gl_Position = vClip;
}
)";
    static const char* kFS = R"(#version 450 core
in vec4 vClip;
uniform mat4 u_InvViewProj;
uniform vec3 u_SunDir;      // light direction (pointing toward ground)
uniform vec3 u_SkyZenith;
uniform vec3 u_SkyHorizon;
uniform vec3 u_SunColor;
out vec4 FragColor;
void main() {
    vec4 worldFar = u_InvViewProj * vClip;
    vec3 ray = normalize(worldFar.xyz / worldFar.w);
    float up = clamp(ray.y, 0.0, 1.0);
    vec3 sky = mix(u_SkyHorizon, u_SkyZenith, pow(up, 0.45));
    // Sun disk (sun is in direction opposite to light)
    vec3 sunPos = normalize(-u_SunDir);
    float sd = dot(ray, sunPos);
    float disk = smoothstep(0.9994, 0.9999, sd);
    sky += u_SunColor * disk * 4.0;
    FragColor = vec4(sky, 1.0);
}
)";
    return m_skyShader.buildFromSource(kVS, kFS);
}

void EditorViewport::drawSky(const glm::mat4& invViewProj, const glm::vec3& sunDir) {
    m_skyShader.bind();
    glUniformMatrix4fv(glGetUniformLocation(m_skyShader.id(), "u_InvViewProj"), 1, GL_FALSE, glm::value_ptr(invViewProj));
    glUniform3fv(glGetUniformLocation(m_skyShader.id(), "u_SunDir"),    1, glm::value_ptr(sunDir));
    glUniform3fv(glGetUniformLocation(m_skyShader.id(), "u_SkyZenith"), 1, glm::value_ptr(m_skyZenith));
    glUniform3fv(glGetUniformLocation(m_skyShader.id(), "u_SkyHorizon"),1, glm::value_ptr(m_skyHorizon));
    glUniform3fv(glGetUniformLocation(m_skyShader.id(), "u_SunColor"),  1, glm::value_ptr(m_sunColor));
    glBindVertexArray(m_skyVao);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);
}

void EditorViewport::recreateFramebuffer() {
    if (m_depthTex) glDeleteTextures(1, &m_depthTex);
    if (m_colorTex) glDeleteTextures(1, &m_colorTex);
    if (m_fbo) glDeleteFramebuffers(1, &m_fbo);

    glCreateFramebuffers(1, &m_fbo);

    glCreateTextures(GL_TEXTURE_2D, 1, &m_colorTex);
    glTextureStorage2D(m_colorTex, 1, GL_RGBA8, m_width, m_height);
    glTextureParameteri(m_colorTex, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTextureParameteri(m_colorTex, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glNamedFramebufferTexture(m_fbo, GL_COLOR_ATTACHMENT0, m_colorTex, 0);

    glCreateTextures(GL_TEXTURE_2D, 1, &m_depthTex);
    glTextureStorage2D(m_depthTex, 1, GL_DEPTH24_STENCIL8, m_width, m_height);
    glTextureParameteri(m_depthTex, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTextureParameteri(m_depthTex, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTextureParameteri(m_depthTex, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(m_depthTex, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTextureParameteri(m_depthTex, GL_TEXTURE_COMPARE_MODE, GL_NONE);
    glNamedFramebufferTexture(m_fbo, GL_DEPTH_STENCIL_ATTACHMENT, m_depthTex, 0);

    GLenum status = glCheckNamedFramebufferStatus(m_fbo, GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        spdlog::error("Viewport FBO incomplete: 0x{:x}", static_cast<unsigned int>(status));
    }
}

} // namespace elysium
