#include "Scripting/ScriptRuntimeSubsystem.hpp"

#include <utility>

namespace elysium {

void ScriptRuntimeSubsystem::setContext(ScriptSystem* scriptSystem,
                                        Scene* scene,
                                        const std::filesystem::path& scriptRoot) {
    m_scriptSystem = scriptSystem;
    m_scene = scene;
    m_scriptRoot = scriptRoot;
}

bool ScriptRuntimeSubsystem::initialize(LogFn logger) {
    m_logger = std::move(logger);
    if (m_scriptSystem == nullptr) {
        return false;
    }

    m_initialized = m_scriptSystem->initialize(m_scriptRoot, m_logger);
    return m_initialized;
}

void ScriptRuntimeSubsystem::shutdown() {
    if (m_scriptSystem != nullptr) {
        m_scriptSystem->shutdown();
    }
    m_initialized = false;
}

void ScriptRuntimeSubsystem::preUpdate(float dt) {
    m_lastDt = dt;
}

void ScriptRuntimeSubsystem::update(float) {
    if (!m_initialized || m_scriptSystem == nullptr || m_scene == nullptr) {
        return;
    }
    m_scriptSystem->update(*m_scene, m_lastDt);
}

void ScriptRuntimeSubsystem::postUpdate(float) {
}

} // namespace elysium
