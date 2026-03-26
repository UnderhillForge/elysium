#include "Physics/PhysicsRuntimeSubsystem.hpp"

#include <utility>

namespace elysium {

void PhysicsRuntimeSubsystem::setContext(PhysicsSystem* physicsSystem,
                                         Scene* scene,
                                         TileMap* tileMap) {
    m_physicsSystem = physicsSystem;
    m_scene = scene;
    m_tileMap = tileMap;
}

bool PhysicsRuntimeSubsystem::initialize(LogFn logger) {
    m_logger = std::move(logger);
    if (m_physicsSystem == nullptr) {
        return false;
    }

    m_initialized = m_physicsSystem->initialize(m_logger);
    return m_initialized;
}

void PhysicsRuntimeSubsystem::shutdown() {
    if (m_physicsSystem != nullptr) {
        m_physicsSystem->shutdown();
    }
    m_initialized = false;
}

void PhysicsRuntimeSubsystem::preUpdate(float dt) {
    m_lastDt = dt;

    if (!m_initialized || m_physicsSystem == nullptr || m_scene == nullptr) {
        return;
    }

    m_physicsSystem->syncScene(*m_scene);
    if (m_tilesDirty && m_tileMap != nullptr) {
        m_physicsSystem->rebuildStaticTiles(*m_tileMap);
        m_tilesDirty = false;
    }
}

void PhysicsRuntimeSubsystem::update(float) {
    if (!m_initialized || m_physicsSystem == nullptr || m_scene == nullptr) {
        return;
    }

    m_physicsSystem->step(*m_scene, m_lastDt);
}

void PhysicsRuntimeSubsystem::postUpdate(float) {
}

} // namespace elysium
