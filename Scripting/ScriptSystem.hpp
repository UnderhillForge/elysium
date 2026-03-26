#pragma once

#include "Scene/Scene.hpp"

#include <filesystem>
#include <functional>
#include <string>
#include <unordered_map>

#include <sol/sol.hpp>

namespace elysium {

class ScriptSystem {
public:
    using LogFn = std::function<void(const std::string&)>;

    bool initialize(const std::filesystem::path& scriptRoot, LogFn logger);
    void shutdown();

    void setScriptRoot(const std::filesystem::path& scriptRoot);

    void update(Scene& scene, float dt);
    bool executeString(const std::string& code);

    bool invokeInteract(SceneEntity& entity);
    bool invokeEnterTrigger(SceneEntity& entity, const std::string& triggerName);

private:
    struct ScriptInstance {
        std::string resolvedPath;
        std::filesystem::file_time_type lastWriteTime{};
        sol::environment environment;
        sol::protected_function onTick;
        sol::protected_function onInteract;
        sol::protected_function onEnterTrigger;

        ScriptInstance(sol::state_view luaState)
            : environment(luaState, sol::create, luaState.globals()) {}
    };

private:
    void bindTypes();
    bool ensureScriptLoaded(SceneEntity& entity);
    std::string resolveScriptPath(const std::string& scriptPath) const;
    void log(const std::string& line) const;
    bool runProtected(sol::protected_function& fn, SceneEntity& entity, float dt);

private:
    std::filesystem::path m_scriptRoot;
    LogFn m_logger;
    sol::state m_lua;
    std::unordered_map<SceneEntityId, ScriptInstance> m_instances;
};

} // namespace elysium
