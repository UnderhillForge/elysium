#include "Scripting/ScriptSystem.hpp"

#include <glm/gtc/quaternion.hpp>
#include <spdlog/spdlog.h>

#include <exception>
#include <utility>

namespace elysium {

bool ScriptSystem::initialize(const std::filesystem::path& scriptRoot, LogFn logger) {
    m_scriptRoot = scriptRoot;
    m_logger = std::move(logger);

    m_lua.open_libraries(
        sol::lib::base,
        sol::lib::package,
        sol::lib::math,
        sol::lib::string,
        sol::lib::table,
        sol::lib::os
    );

    bindTypes();
    log("Lua ScriptSystem initialized.");
    return true;
}

void ScriptSystem::shutdown() {
    m_instances.clear();
    m_lua = sol::state{};
}

void ScriptSystem::setScriptRoot(const std::filesystem::path& scriptRoot) {
    m_scriptRoot = scriptRoot;
}

void ScriptSystem::bindTypes() {
    m_lua.set_function("log", [this](const std::string& message) {
        log("[lua] " + message);
    });

    m_lua.new_usertype<glm::vec3>(
        "Vector3",
        sol::constructors<glm::vec3(), glm::vec3(float, float, float)>(),
        "x", &glm::vec3::x,
        "y", &glm::vec3::y,
        "z", &glm::vec3::z
    );

    m_lua.new_usertype<glm::quat>(
        "Quaternion",
        sol::constructors<glm::quat(), glm::quat(float, float, float, float)>(),
        "w", &glm::quat::w,
        "x", &glm::quat::x,
        "y", &glm::quat::y,
        "z", &glm::quat::z
    );

    m_lua.new_usertype<Transform>(
        "Transform",
        "position", &Transform::position,
        "rotation_euler_deg", &Transform::rotationEulerDeg,
        "scale", &Transform::scale
    );

    m_lua.new_usertype<SceneEntity::ScriptComponent>(
        "ScriptComponent",
        "enabled", &SceneEntity::ScriptComponent::enabled,
        "script_path", &SceneEntity::ScriptComponent::scriptPath
    );

    m_lua.new_usertype<SceneEntity>(
        "Entity",
        "id", sol::readonly(&SceneEntity::id),
        "name", &SceneEntity::name,
        "transform", &SceneEntity::transform,
        "tint", &SceneEntity::tint
    );
}

std::string ScriptSystem::resolveScriptPath(const std::string& scriptPath) const {
    std::filesystem::path path{scriptPath};
    if (path.is_relative()) {
        path = m_scriptRoot / path;
    }
    return path.lexically_normal().string();
}

void ScriptSystem::log(const std::string& line) const {
    if (m_logger) {
        m_logger(line);
    } else {
        spdlog::info("{}", line);
    }
}

bool ScriptSystem::ensureScriptLoaded(SceneEntity& entity) {
    if (!entity.script.has_value() || !entity.script->enabled || entity.script->scriptPath.empty()) {
        return false;
    }

    const std::string resolvedPath = resolveScriptPath(entity.script->scriptPath);
    const std::filesystem::path fsPath{resolvedPath};
    if (!std::filesystem::exists(fsPath)) {
        log("Script missing: " + resolvedPath);
        return false;
    }

    auto it = m_instances.find(entity.id);
    const auto lastWriteTime = std::filesystem::last_write_time(fsPath);
    const bool requiresReload =
        (it == m_instances.end()) ||
        it->second.resolvedPath != resolvedPath ||
        it->second.lastWriteTime != lastWriteTime;

    if (!requiresReload) {
        return true;
    }

    ScriptInstance instance{m_lua};
    instance.resolvedPath = resolvedPath;
    instance.lastWriteTime = lastWriteTime;

    sol::load_result loadedChunk = m_lua.load_file(resolvedPath);
    if (!loadedChunk.valid()) {
        const sol::error err = loadedChunk;
        log("Lua load error: " + std::string(err.what()));
        return false;
    }

    sol::protected_function chunk = loadedChunk;
    sol::protected_function_result execResult = chunk(instance.environment);
    if (!execResult.valid()) {
        const sol::error err = execResult;
        log("Lua runtime error: " + std::string(err.what()));
        return false;
    }

    instance.onTick = instance.environment["on_tick"];
    instance.onInteract = instance.environment["on_interact"];
    instance.onEnterTrigger = instance.environment["on_enter_trigger"];

    m_instances.insert_or_assign(entity.id, std::move(instance));
    log("Loaded script: " + resolvedPath + " (entity " + std::to_string(entity.id) + ")");
    return true;
}

bool ScriptSystem::runProtected(sol::protected_function& fn, SceneEntity& entity, float dt) {
    if (!fn.valid()) {
        return false;
    }

    sol::protected_function_result result = fn(entity, dt);
    if (!result.valid()) {
        const sol::error err = result;
        log("Lua callback error (entity " + std::to_string(entity.id) + "): " + std::string(err.what()));
        return false;
    }
    return true;
}

void ScriptSystem::update(Scene& scene, float dt) {
    for (auto& entity : scene.entities()) {
        if (!entity.script.has_value() || !entity.script->enabled || entity.script->scriptPath.empty()) {
            m_instances.erase(entity.id);
            continue;
        }

        if (!ensureScriptLoaded(entity)) {
            continue;
        }

        auto it = m_instances.find(entity.id);
        if (it == m_instances.end()) {
            continue;
        }

        runProtected(it->second.onTick, entity, dt);
    }
}

bool ScriptSystem::executeString(const std::string& code) {
    sol::protected_function_result result = m_lua.safe_script(code, &sol::script_pass_on_error);
    if (!result.valid()) {
        const sol::error err = result;
        log("Lua console error: " + std::string(err.what()));
        return false;
    }

    log("Lua console executed.");
    return true;
}

bool ScriptSystem::invokeInteract(SceneEntity& entity) {
    if (!ensureScriptLoaded(entity)) {
        return false;
    }

    auto it = m_instances.find(entity.id);
    if (it == m_instances.end() || !it->second.onInteract.valid()) {
        return false;
    }

    sol::protected_function_result result = it->second.onInteract(entity);
    if (!result.valid()) {
        const sol::error err = result;
        log("Lua on_interact error: " + std::string(err.what()));
        return false;
    }

    return true;
}

bool ScriptSystem::invokeEnterTrigger(SceneEntity& entity, const std::string& triggerName) {
    if (!ensureScriptLoaded(entity)) {
        return false;
    }

    auto it = m_instances.find(entity.id);
    if (it == m_instances.end() || !it->second.onEnterTrigger.valid()) {
        return false;
    }

    sol::protected_function_result result = it->second.onEnterTrigger(entity, triggerName);
    if (!result.valid()) {
        const sol::error err = result;
        log("Lua on_enter_trigger error: " + std::string(err.what()));
        return false;
    }

    return true;
}

} // namespace elysium
