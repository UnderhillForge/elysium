#pragma once

#include "Foundation/Subsystem.hpp"
#include "Physics/PhysicsSystem.hpp"
#include "Scene/Scene.hpp"
#include "Tile/TileMap.hpp"

namespace elysium {

// Inspired by RavEngine: physics simulation and scene sync live behind an
// explicit update-phase boundary for deterministic gameplay/network hooks.
class PhysicsRuntimeSubsystem final : public Subsystem {
public:
    const char* name() const override { return "PhysicsRuntimeSubsystem"; }

    void setContext(PhysicsSystem* physicsSystem, Scene* scene, TileMap* tileMap);
    void setTilesDirty(bool dirty) { m_tilesDirty = dirty; }

    bool initialize(LogFn logger) override;
    void shutdown() override;
    void preUpdate(float dt) override;
    void update(float dt) override;
    void postUpdate(float dt) override;

private:
    LogFn m_logger;
    PhysicsSystem* m_physicsSystem{nullptr};
    Scene* m_scene{nullptr};
    TileMap* m_tileMap{nullptr};
    float m_lastDt{0.0f};
    bool m_tilesDirty{true};
    bool m_initialized{false};
};

} // namespace elysium
