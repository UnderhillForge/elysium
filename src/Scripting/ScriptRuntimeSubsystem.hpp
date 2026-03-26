#pragma once

#include "Foundation/Subsystem.hpp"
#include "Scripting/ScriptSystem.hpp"
#include "Scene/Scene.hpp"

#include <filesystem>

namespace elysium {

// Inspired by RavEngine: keep script runtime in an explicit subsystem layer,
// while reusing the existing ScriptSystem implementation.
class ScriptRuntimeSubsystem final : public Subsystem {
public:
    const char* name() const override { return "ScriptRuntimeSubsystem"; }

    void setContext(ScriptSystem* scriptSystem,
                    Scene* scene,
                    const std::filesystem::path& scriptRoot);

    bool initialize(LogFn logger) override;
    void shutdown() override;
    void preUpdate(float dt) override;
    void update(float dt) override;
    void postUpdate(float dt) override;

private:
    LogFn m_logger;
    ScriptSystem* m_scriptSystem{nullptr};
    Scene* m_scene{nullptr};
    std::filesystem::path m_scriptRoot;
    float m_lastDt{0.0f};
    bool m_initialized{false};
};

} // namespace elysium
