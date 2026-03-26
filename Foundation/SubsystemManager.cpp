#include "Foundation/SubsystemManager.hpp"

namespace elysium {

bool SubsystemManager::registerSubsystem(std::unique_ptr<Subsystem> subsystem) {
    if (!subsystem) {
        return false;
    }

    const std::string key = subsystem->name();
    if (key.empty() || m_byName.contains(key)) {
        return false;
    }

    Subsystem* raw = subsystem.get();
    m_ordered.push_back(std::move(subsystem));
    m_byName.emplace(key, raw);
    return true;
}

bool SubsystemManager::initializeAll(LogFn logger) {
    if (m_initialized) {
        return true;
    }

    for (auto& subsystem : m_ordered) {
        if (!subsystem->initialize(logger)) {
            shutdownAll();
            return false;
        }
    }

    m_initialized = true;
    return true;
}

void SubsystemManager::shutdownAll() {
    for (auto it = m_ordered.rbegin(); it != m_ordered.rend(); ++it) {
        (*it)->shutdown();
    }
    m_initialized = false;
}

void SubsystemManager::preUpdateAll(float dt) {
    for (auto& subsystem : m_ordered) {
        subsystem->preUpdate(dt);
    }
}

void SubsystemManager::updateAll(float dt) {
    for (auto& subsystem : m_ordered) {
        subsystem->update(dt);
    }
}

void SubsystemManager::postUpdateAll(float dt) {
    for (auto& subsystem : m_ordered) {
        subsystem->postUpdate(dt);
    }
}

Subsystem* SubsystemManager::find(const std::string& name) {
    auto it = m_byName.find(name);
    return (it != m_byName.end()) ? it->second : nullptr;
}

} // namespace elysium
