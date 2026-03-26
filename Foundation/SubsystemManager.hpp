#pragma once

#include "Foundation/Subsystem.hpp"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace elysium {

class SubsystemManager {
public:
    using LogFn = Subsystem::LogFn;

    bool registerSubsystem(std::unique_ptr<Subsystem> subsystem);
    bool initializeAll(LogFn logger);
    void shutdownAll();

    void preUpdateAll(float dt);
    void updateAll(float dt);
    void postUpdateAll(float dt);

    Subsystem* find(const std::string& name);

private:
    std::vector<std::unique_ptr<Subsystem>> m_ordered;
    std::unordered_map<std::string, Subsystem*> m_byName;
    bool m_initialized{false};
};

} // namespace elysium
