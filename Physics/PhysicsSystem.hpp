#pragma once

#include "Scene/Scene.hpp"
#include "Tile/TileMap.hpp"

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <Jolt/Jolt.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyID.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayerInterfaceTable.h>
#include <Jolt/Physics/Collision/BroadPhase/ObjectVsBroadPhaseLayerFilterTable.h>
#include <Jolt/Physics/Collision/ObjectLayerPairFilterTable.h>
#include <Jolt/Physics/PhysicsSystem.h>

namespace elysium {

class PhysicsSystem {
public:
    using LogFn = std::function<void(const std::string&)>;

    ~PhysicsSystem();

    bool initialize(LogFn logger);
    void shutdown();

    void setSimulationEnabled(bool enabled) { m_simulationEnabled = enabled; }
    bool simulationEnabled() const { return m_simulationEnabled; }
    void requestSingleStep() { m_singleStepRequested = true; }

    void syncScene(Scene& scene);
    void rebuildStaticTiles(const TileMap& tileMap);
    void step(Scene& scene, float dt);

    std::vector<glm::vec3> buildDebugLines() const;
    std::size_t bodyCount() const;

private:
    struct EntityBodyState {
        JPH::BodyID bodyId;
        bool dynamic{true};
    };

private:
    void log(const std::string& line) const;
    glm::vec3 defaultHalfExtentsFor(const SceneEntity& entity) const;
    void addBoxLines(std::vector<glm::vec3>& lines, const JPH::BodyID& bodyId) const;

private:
    LogFn m_logger;

    std::unique_ptr<JPH::TempAllocatorImpl> m_tempAllocator;
    std::unique_ptr<JPH::JobSystemThreadPool> m_jobSystem;
    std::unique_ptr<JPH::PhysicsSystem> m_physics;
    std::unique_ptr<JPH::BroadPhaseLayerInterfaceTable> m_bpLayerInterface;
    std::unique_ptr<JPH::ObjectLayerPairFilterTable> m_objectLayerPairFilter;
    std::unique_ptr<JPH::ObjectVsBroadPhaseLayerFilterTable> m_objectVsBroadPhaseFilter;

    std::unordered_map<SceneEntityId, EntityBodyState> m_entityBodies;
    std::vector<JPH::BodyID> m_tileBodies;
    std::unordered_map<std::uint32_t, glm::vec3> m_bodyHalfExtents;

    bool m_initialized{false};
    bool m_simulationEnabled{true};
    bool m_singleStepRequested{false};
};

} // namespace elysium
