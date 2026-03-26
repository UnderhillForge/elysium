#pragma once

#include "Assets/AssetCatalog.hpp"
#include "Assets/GLTFLoader.hpp"
#include "Editor/EditorViewport.hpp"
#include "Foundation/SubsystemManager.hpp"
#include "Particles/ParticleSystem.hpp"
#include "Physics/PhysicsSystem.hpp"
#include "Physics/PhysicsRuntimeSubsystem.hpp"
#include "Procedural/ProceduralTileGenerator.hpp"
#include "Scene/Scene.hpp"
#include "Scripting/ScriptSystem.hpp"
#include "Scripting/ScriptRuntimeSubsystem.hpp"
#include "Audio/SpatialAudioSystem.hpp"
#include "Networking/NetworkingSystem.hpp"
#include "Tile/TileMap.hpp"
#include "Walkmesh/CharacterController.hpp"
#include "Walkmesh/Walkmesh.hpp"

#include <SDL.h>
#include <array>
#include <chrono>
#include <deque>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace elysium {

class Application {
public:
    Application() = default;
    ~Application();

    enum class GizmoMode {
        Translate,
        Rotate,
        Scale
    };

    enum class GizmoAxis {
        None,
        X,
        Y,
        Z
    };

    enum class CollisionPlaneHandle {
        None,
        Offset,
        Normal
    };

    enum class DockPreset {
        Default,
        Terrain,
        AssetPrep
    };

    bool initialize(int argc, char** argv);
    void run();
    void shutdown();

private:
    void pollEvents();
    void beginImGuiFrame();
    void drawMainDockspace();
    void applyDockPreset(ImGuiID dockspaceID, DockPreset preset);
    void drawViewportPanel(float dt);
    void drawAssetsPanel();
    void drawOutlinerPanel();
    void drawPropertiesPanel();
    void drawConsolePanel();
    void drawSettingsWindow();
    void endImGuiFrame();

    void rebuildFonts();

    void appendLog(const std::string& line);
    void loadStartupModelIfAny(int argc, char** argv);
    void instantiateModel(const std::filesystem::path& filePath);
    void handleDroppedPath(const std::filesystem::path& path);
    void snapEntityToGrid(SceneEntity& entity) const;
    bool loadSplashTexture(const std::filesystem::path& filePath);
    void destroySplashTexture();
    void clearAssetThumbnailCache();
    bool showStartupSplash(std::chrono::milliseconds duration);
    void handleViewportShortcuts();
    void handleViewportSelectionAndGizmo(const ImVec2& imageMin, const ImVec2& imageMax);
    void drawViewportGizmoOverlay(const ImVec2& imageMin, const ImVec2& imageMax);
    void drawProceduralPreviewOverlay(const ImVec2& imageMin, const ImVec2& imageMax);

    // Rebuild the CPU walkmesh from the current TileMap and, if walkmesh debug is
    // enabled, upload the wireframe lines to the viewport GPU buffer.
    void rebuildWalkmesh();
    void syncParticleEmitters();
    void syncSpatialAudio();
    void regenerateProceduralPreview();
    void applyProceduralPreviewToTileMap();
    void clearProceduralPreview();
    void clearLastAppliedProceduralArea();

    // Draw the 2-D character avatar overlay onto the viewport ImGui image.
    void drawCharacterPreview(const ImVec2& imageMin, const ImVec2& imageMax);
    void drawLuaConsole();

    struct SavedEntity {
        std::string name;
        std::string assetPath;
        bool scriptEnabled{false};
        std::string scriptPath;
        bool physicsEnabled{false};
        bool physicsDynamic{true};
        glm::vec3 physicsHalfExtents{0.5f, 0.5f, 0.5f};
        Transform transform{};
        glm::vec4 tint{1.0f};
        // Light component
        bool      hasLight{false};
        int       lightType{0};   // 0=Point, 1=Directional
        bool      lightEnabled{true};
        glm::vec3 lightColor{1.0f, 1.0f, 1.0f};
        float     lightIntensity{1.0f};
        float     lightRange{10.0f};
        // Particle emitter component
        bool      hasParticleEmitter{false};
        bool      particleEmitterEnabled{true};
        int       particleEmitterPreset{0};
        int       particleEmitterBlendMode{0};
        uint32_t  particleEmitterMaxParticles{10000};
        float     particleEmitterEmissionRate{100.0f};
        float     particleEmitterLifetime{2.0f};
        glm::vec3 particleEmitterVelocity{0.0f, 1.0f, 0.0f};
        glm::vec3 particleEmitterVelocityVariance{0.5f, 0.2f, 0.5f};
        glm::vec3 particleEmitterPositionVariance{0.1f, 0.1f, 0.1f};
        glm::vec3 particleEmitterAcceleration{0.0f, -0.98f, 0.0f};
        float     particleEmitterDamping{0.99f};
        bool      particleEmitterLooping{true};
        uint32_t  particleEmitterBurstCount{0};
        float     particleEmitterBurstInterval{1.0f};
        bool      particleEmitterBurstOnStart{false};
        glm::vec4 particleEmitterColorStart{1.0f, 1.0f, 1.0f, 1.0f};
        glm::vec4 particleEmitterColorMid{1.0f, 1.0f, 1.0f, 0.65f};
        glm::vec4 particleEmitterColorEnd{1.0f, 1.0f, 1.0f, 0.0f};
        float     particleEmitterColorMidPoint{0.45f};
        float     particleEmitterSizeStart{0.1f};
        float     particleEmitterSizeMid{0.08f};
        float     particleEmitterSizeEnd{0.05f};
        float     particleEmitterSizeMidPoint{0.45f};
        int       particleEmitterSpriteColumns{1};
        int       particleEmitterSpriteRows{1};
        float     particleEmitterSpriteFrameRate{0.0f};
        bool      particleEmitterRandomStartFrame{false};
        std::string particleEmitterSpriteTexturePath;
        bool      particleEmitterCollideWithGround{false};
        glm::vec3 particleEmitterCollisionPlaneNormal{0.0f, 1.0f, 0.0f};
        float     particleEmitterCollisionPlaneOffset{0.0f};
        float     particleEmitterBounceFactor{0.35f};
        float     particleEmitterCollisionFade{0.45f};
        int       particleEmitterMaxBounces{1};
        // Authored spatial audio component
        bool      hasAudio{false};
        bool      audioEnabled{true};
        std::string audioClipPath;
        float     audioGain{1.0f};
        float     audioMinDistance{1.0f};
        float     audioMaxDistance{40.0f};
        bool      audioLooping{false};

        bool operator==(const SavedEntity& rhs) const {
            return name == rhs.name &&
                   assetPath == rhs.assetPath &&
                   scriptEnabled == rhs.scriptEnabled &&
                   scriptPath == rhs.scriptPath &&
                   physicsEnabled == rhs.physicsEnabled &&
                   physicsDynamic == rhs.physicsDynamic &&
                   physicsHalfExtents == rhs.physicsHalfExtents &&
                   transform.position == rhs.transform.position &&
                   transform.rotationEulerDeg == rhs.transform.rotationEulerDeg &&
                   transform.scale == rhs.transform.scale &&
                   tint == rhs.tint &&
                   hasLight == rhs.hasLight &&
                   lightType == rhs.lightType &&
                   lightEnabled == rhs.lightEnabled &&
                   lightColor == rhs.lightColor &&
                   lightIntensity == rhs.lightIntensity &&
                   lightRange == rhs.lightRange &&
                   hasParticleEmitter == rhs.hasParticleEmitter &&
                   particleEmitterEnabled == rhs.particleEmitterEnabled &&
                   particleEmitterPreset == rhs.particleEmitterPreset &&
                   particleEmitterBlendMode == rhs.particleEmitterBlendMode &&
                   particleEmitterMaxParticles == rhs.particleEmitterMaxParticles &&
                   particleEmitterEmissionRate == rhs.particleEmitterEmissionRate &&
                   particleEmitterLifetime == rhs.particleEmitterLifetime &&
                   particleEmitterVelocity == rhs.particleEmitterVelocity &&
                   particleEmitterVelocityVariance == rhs.particleEmitterVelocityVariance &&
                   particleEmitterPositionVariance == rhs.particleEmitterPositionVariance &&
                   particleEmitterAcceleration == rhs.particleEmitterAcceleration &&
                   particleEmitterDamping == rhs.particleEmitterDamping &&
                   particleEmitterLooping == rhs.particleEmitterLooping &&
                   particleEmitterBurstCount == rhs.particleEmitterBurstCount &&
                   particleEmitterBurstInterval == rhs.particleEmitterBurstInterval &&
                   particleEmitterBurstOnStart == rhs.particleEmitterBurstOnStart &&
                   particleEmitterColorStart == rhs.particleEmitterColorStart &&
                   particleEmitterColorMid == rhs.particleEmitterColorMid &&
                   particleEmitterColorEnd == rhs.particleEmitterColorEnd &&
                   particleEmitterColorMidPoint == rhs.particleEmitterColorMidPoint &&
                   particleEmitterSizeStart == rhs.particleEmitterSizeStart &&
                   particleEmitterSizeMid == rhs.particleEmitterSizeMid &&
                   particleEmitterSizeEnd == rhs.particleEmitterSizeEnd &&
                   particleEmitterSizeMidPoint == rhs.particleEmitterSizeMidPoint &&
                   particleEmitterSpriteColumns == rhs.particleEmitterSpriteColumns &&
                   particleEmitterSpriteRows == rhs.particleEmitterSpriteRows &&
                   particleEmitterSpriteFrameRate == rhs.particleEmitterSpriteFrameRate &&
                   particleEmitterRandomStartFrame == rhs.particleEmitterRandomStartFrame &&
                   particleEmitterSpriteTexturePath == rhs.particleEmitterSpriteTexturePath &&
                   particleEmitterCollideWithGround == rhs.particleEmitterCollideWithGround &&
                   particleEmitterCollisionPlaneNormal == rhs.particleEmitterCollisionPlaneNormal &&
                   particleEmitterCollisionPlaneOffset == rhs.particleEmitterCollisionPlaneOffset &&
                   particleEmitterBounceFactor == rhs.particleEmitterBounceFactor &&
                   particleEmitterCollisionFade == rhs.particleEmitterCollisionFade &&
                   particleEmitterMaxBounces == rhs.particleEmitterMaxBounces &&
                   hasAudio == rhs.hasAudio &&
                   audioEnabled == rhs.audioEnabled &&
                   audioClipPath == rhs.audioClipPath &&
                   audioGain == rhs.audioGain &&
                   audioMinDistance == rhs.audioMinDistance &&
                   audioMaxDistance == rhs.audioMaxDistance &&
                   audioLooping == rhs.audioLooping;
            }
    };

    struct SavedTile {
        int x{0};
        int z{0};
        TileData data{};

        bool operator==(const SavedTile& rhs) const {
            return x == rhs.x && z == rhs.z && data.tint == rhs.data.tint && data.walkable == rhs.data.walkable;
        }
    };

    struct EditorStateSnapshot {
        std::vector<SavedEntity> entities;
        std::vector<SavedTile> tiles;
        float tileSize{1.0f};
        bool gridEnabled{true};
        bool snapNewPlacements{true};
        bool tileBrushEnabled{false};
        bool brushWalkable{true};
        glm::vec4 tileBrushTint{0.60f, 0.68f, 0.74f, 1.0f};
    };

    EditorStateSnapshot makeSnapshot() const;
    bool snapshotsEqual(const EditorStateSnapshot& lhs, const EditorStateSnapshot& rhs) const;
    void applySnapshot(const EditorStateSnapshot& snapshot);
    void captureUndoSnapshot(const std::string& reason);
    void clearHistory();
    bool undo();
    bool redo();

    bool saveAreaToFile(const std::filesystem::path& filePath);
    bool loadAreaFromFile(const std::filesystem::path& filePath);
    void newArea();

private:
    SDL_Window* m_window{nullptr};
    SDL_GLContext m_glContext{nullptr};
    bool m_running{false};

    int m_windowWidth{1600};
    int m_windowHeight{900};

    EditorViewport m_viewport;
    Scene m_scene;
    TileMap m_tileMap;
    GLTFLoader m_loader;

    std::unordered_map<std::string, std::shared_ptr<GLTFModel>> m_modelCache;
    std::unordered_map<std::string, GLuint> m_assetThumbnailCache;
    std::unordered_set<std::string> m_failedAssetThumbnails;
    AssetCatalog m_assetCatalog;
    SubsystemManager m_subsystems;
    ScriptSystem m_scriptSystem;
    PhysicsSystem m_physicsSystem;
    ScriptRuntimeSubsystem* m_scriptRuntimeSubsystem{nullptr};
    PhysicsRuntimeSubsystem* m_physicsRuntimeSubsystem{nullptr};
    SpatialAudioSystem* m_spatialAudioSubsystem{nullptr};
    NetworkingSystem* m_networkingSubsystem{nullptr};
    std::array<char, 512> m_assetRootInput{};
    std::array<char, 128> m_assetFilterInput{};
    std::array<char, 1024> m_luaConsoleInput{};

    std::vector<std::string> m_consoleLines;
    std::optional<SceneEntityId> m_selectedEntityId;

    GLuint m_splashTexture{0};
    int m_splashWidth{0};
    int m_splashHeight{0};

    bool m_gridEnabled{true};
    bool m_snapNewPlacements{true};
    float m_gridSnapSize{1.0f};

    bool m_tileBrushEnabled{false};
    bool m_brushWalkable{true};          // new tiles placed as walkable by default
    glm::vec4 m_tileBrushTint{0.60f, 0.68f, 0.74f, 1.0f};

    // Procedural area editor state.
    ProceduralTileGenerator m_proceduralTileGenerator;
    ProceduralAreaParams m_proceduralParams{};
    std::optional<GeneratedArea> m_proceduralPreviewArea;
    bool m_proceduralOverlayEnabled{true};
    struct ProceduralTileRestore {
        int x{0};
        int z{0};
        bool hadPreviousTile{false};
        TileData previousTile{};
    };
    std::vector<ProceduralTileRestore> m_lastAppliedProceduralRestore;

    // Walkmesh & character preview
    Walkmesh            m_tileWalkmesh;
    CharacterController m_controller;
    bool m_walkmeshPreviewEnabled{false};
    bool m_walkmeshDebugEnabled{false};
    bool m_walkmeshDirty{true};  // triggers rebuild on next drawViewportPanel()
    bool m_physicsDebugEnabled{false};
    bool m_viewportDebugOverlayEnabled{true};

    // UI appearance settings
    float m_uiFontSize{26.0f};   // default 2× the ImGui built-in 13 px
    bool  m_fontsDirty{false};   // triggers atlas rebuild before next frame
    bool  m_showSettingsWindow{false};

    // FPS tracking
    float m_fpsSmoothed{0.0f};
    float m_frameTimeMs{0.0f};

    GizmoMode m_gizmoMode{GizmoMode::Translate};
    GizmoAxis m_activeGizmoAxis{GizmoAxis::None};
    CollisionPlaneHandle m_activeCollisionPlaneHandle{CollisionPlaneHandle::None};
    glm::vec3 m_lastGizmoGroundPoint{0.0f};
    ImVec2 m_lastGizmoMousePos{0.0f, 0.0f};
    ImVec2 m_lastCollisionPlaneMousePos{0.0f, 0.0f};

    bool m_dockLayoutInitialized{false};
    bool m_rebuildDockLayoutRequested{false};
    DockPreset m_activeDockPreset{DockPreset::Default};

    std::filesystem::path m_currentAreaPath{"area.json"};
    std::array<char, 512> m_areaPathInput{};
    std::vector<EditorStateSnapshot> m_undoStack;
    std::vector<EditorStateSnapshot> m_redoStack;
    std::unordered_map<SceneEntityId, std::shared_ptr<ParticleEmitter>> m_particleEmitters;
    std::size_t m_maxHistoryEntries{128};
    bool m_suppressHistory{false};
    bool m_propertyEditActive{false};
};

} // namespace elysium
