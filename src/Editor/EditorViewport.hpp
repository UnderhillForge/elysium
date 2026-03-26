#pragma once

#include "Renderer/Camera.hpp"
#include "Renderer/Shader.hpp"
#include "Scene/Scene.hpp"
#include "Tile/TileMap.hpp"

#include <glad/glad.h>
#include <imgui.h>

#include <vector>

namespace elysium {

class EditorViewport {
public:
    enum class MaterialDebugView {
        Lit = 0,
        Normals,
        Roughness,
        Metallic,
        Emissive
    };

    bool initialize();
    void shutdown();

    void resize(int width, int height);
    void updateCamera(float dt, bool hovered, bool focused);
    void render(const Scene& scene, const TileMap& tileMap);
    void setGridEnabled(bool enabled) { m_gridEnabled = enabled; }

    // Walkmesh debug overlay. Call uploadWalkmeshLines() whenever the walkmesh
    // is rebuilt, then toggle visibility with setWalkmeshDebugEnabled().
    void uploadWalkmeshLines(const std::vector<glm::vec3>& walkableLines,
                             const std::vector<glm::vec3>& blockedLines);
    void setWalkmeshDebugEnabled(bool enabled) { m_drawWalkmesh = enabled; }
    void uploadPhysicsLines(const std::vector<glm::vec3>& lines);
    void setPhysicsDebugEnabled(bool enabled) { m_drawPhysics = enabled; }
    void setDrawSky(bool enabled)              { m_drawSky = enabled; }
    bool drawSkyEnabled()               const  { return m_drawSky; }
    void setSkyZenith(const glm::vec3& c)      { m_skyZenith  = c; }
    void setSkyHorizon(const glm::vec3& c)     { m_skyHorizon = c; }
    void setSunColor(const glm::vec3& c)       { m_sunColor   = c; }
    void setAmbientColor(const glm::vec3& c)   { m_ambientColor = c; }
    const glm::vec3& skyZenith()   const { return m_skyZenith; }
    const glm::vec3& skyHorizon()  const { return m_skyHorizon; }
    const glm::vec3& sunColor()    const { return m_sunColor; }
    const glm::vec3& ambientColor()const { return m_ambientColor; }

    void setMaterialDebugView(MaterialDebugView view) { m_materialDebugView = view; }
    MaterialDebugView materialDebugView() const { return m_materialDebugView; }
    void setToneMappingEnabled(bool enabled) { m_toneMappingEnabled = enabled; }
    bool toneMappingEnabled() const { return m_toneMappingEnabled; }
    void setExposure(float exposure) { m_exposure = exposure; }
    float exposure() const { return m_exposure; }
    void setGamma(float gamma) { m_gamma = gamma; }
    float gamma() const { return m_gamma; }

    // Camera accessors for external systems (e.g. CharacterController).
    glm::vec3 cameraPosition() const { return m_camera.position(); }
    glm::vec3 cameraForward() const { return m_camera.forwardVector(); }
    glm::vec3 cameraUp() const { return m_camera.upVector(); }
    float cameraYawDeg() const { return m_camera.yawDeg(); }
    bool screenPointToGround(const ImVec2& mousePos, const ImVec2& imageMin, const ImVec2& imageMax, glm::vec3& outGroundPoint) const;
    bool worldToScreen(const glm::vec3& worldPoint, const ImVec2& imageMin, const ImVec2& imageMax, ImVec2& outScreenPoint) const;

    GLuint colorTexture() const { return m_colorTex; }
    GLuint depthTexture() const { return m_depthTex; }

private:
    bool buildShaders();
    bool buildGridShader();
    bool buildTileShader();
        bool buildSkyShader();
    void buildGridGeometry();
    void buildTileGeometry();
    void drawGrid(const glm::mat4& viewProj);
    void drawTiles(const glm::mat4& viewProj, const TileMap& tileMap);
    void drawWalkmeshDebug(const glm::mat4& viewProj);
    void drawPhysicsDebug(const glm::mat4& viewProj);
        void drawSky(const glm::mat4& invViewProj, const glm::vec3& sunDir);
    void recreateFramebuffer();

private:
    int m_width{1280};
    int m_height{720};

    GLuint m_fbo{0};
    GLuint m_colorTex{0};
    GLuint m_depthTex{0};

    GLuint m_gridVao{0};
    GLuint m_gridVbo{0};
    GLsizei m_gridVertexCount{0};

    GLuint m_tileVao{0};
    GLuint m_tileVbo{0};
    GLuint m_tileIbo{0};
    GLuint m_tileInstanceVbo{0};
    GLsizei m_tileIndexCount{0};

    // Walkmesh debug wireframe: two draw ranges in one VBO.
    // Walkable lines drawn green, blocked lines drawn red.
    GLuint m_walkmeshVao{0};
    GLuint m_walkmeshVbo{0};
    GLsizei m_walkmeshWalkableCount{0};  // vertex count for walkable section
    GLsizei m_walkmeshBlockedOffset{0};  // first vertex of blocked section
    GLsizei m_walkmeshBlockedCount{0};
    bool m_drawWalkmesh{false};

    GLuint m_physicsVao{0};
    GLuint m_physicsVbo{0};
    GLsizei m_physicsLineVertexCount{0};
    bool m_drawPhysics{false};

    Camera m_camera;
    Shader m_shader;
    Shader m_gridShader;
    Shader m_tileShader;
    Shader    m_skyShader;
    GLuint    m_skyVao{0};

    bool m_gridEnabled{true};

    bool      m_drawSky{true};
    glm::vec3 m_skyZenith  {0.10f, 0.18f, 0.36f};
    glm::vec3 m_skyHorizon {0.55f, 0.70f, 0.80f};
    glm::vec3 m_sunColor   {1.0f,  0.94f, 0.78f};
    glm::vec3 m_ambientColor{0.06f, 0.07f, 0.10f};
    MaterialDebugView m_materialDebugView{MaterialDebugView::Lit};
    bool m_toneMappingEnabled{true};
    float m_exposure{1.0f};
    float m_gamma{2.2f};
};

} // namespace elysium
