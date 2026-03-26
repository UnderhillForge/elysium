#include "Physics/PhysicsSystem.hpp"

#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystem.h>
#include <Jolt/Core/Memory.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Physics/Body/BodyActivationListener.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Body/MotionProperties.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayerInterfaceTable.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>
#include <Jolt/Physics/Collision/ObjectLayerPairFilterTable.h>
#include <Jolt/Physics/Collision/BroadPhase/ObjectVsBroadPhaseLayerFilterTable.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/PhysicsSystem.h>

#include <glm/glm.hpp>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <thread>
#include <unordered_set>

namespace elysium {

namespace {

constexpr JPH::ObjectLayer kNonMovingLayer = 0;
constexpr JPH::ObjectLayer kMovingLayer = 1;
constexpr JPH::uint kNumObjectLayers = 2;

namespace BroadPhaseLayers {
    static constexpr JPH::BroadPhaseLayer NON_MOVING(0);
    static constexpr JPH::BroadPhaseLayer MOVING(1);
    static constexpr JPH::uint NUM_LAYERS(2);
};

JPH::RVec3 toJolt(const glm::vec3& v) {
    return JPH::RVec3(v.x, v.y, v.z);
}

glm::vec3 toGlm(const JPH::RVec3& v) {
    return glm::vec3(static_cast<float>(v.GetX()), static_cast<float>(v.GetY()), static_cast<float>(v.GetZ()));
}

glm::vec3 rotateByQuat(const JPH::Quat& q, const glm::vec3& v) {
    const JPH::Vec3 r = q * JPH::Vec3(v.x, v.y, v.z);
    return glm::vec3(r.GetX(), r.GetY(), r.GetZ());
}

} // namespace

PhysicsSystem::~PhysicsSystem() {
    shutdown();
}

bool PhysicsSystem::initialize(LogFn logger) {
    if (m_initialized) {
        return true;
    }

    m_logger = std::move(logger);

    JPH::RegisterDefaultAllocator();
    JPH::Factory::sInstance = new JPH::Factory();
    JPH::RegisterTypes();

    constexpr JPH::uint cMaxBodies = 8192;
    constexpr JPH::uint cNumBodyMutexes = 0;
    constexpr JPH::uint cMaxBodyPairs = 8192;
    constexpr JPH::uint cMaxContactConstraints = 8192;

    m_tempAllocator = std::make_unique<JPH::TempAllocatorImpl>(10 * 1024 * 1024);
    m_jobSystem = std::make_unique<JPH::JobSystemThreadPool>(
        JPH::cMaxPhysicsJobs,
        JPH::cMaxPhysicsBarriers,
        std::max(1u, std::thread::hardware_concurrency() > 1 ? std::thread::hardware_concurrency() - 1 : 1)
    );
    m_physics = std::make_unique<JPH::PhysicsSystem>();

    m_bpLayerInterface = std::make_unique<JPH::BroadPhaseLayerInterfaceTable>(kNumObjectLayers, BroadPhaseLayers::NUM_LAYERS);
    m_bpLayerInterface->MapObjectToBroadPhaseLayer(kNonMovingLayer, BroadPhaseLayers::NON_MOVING);
    m_bpLayerInterface->MapObjectToBroadPhaseLayer(kMovingLayer, BroadPhaseLayers::MOVING);

    m_objectLayerPairFilter = std::make_unique<JPH::ObjectLayerPairFilterTable>(kNumObjectLayers);
    m_objectLayerPairFilter->EnableCollision(kMovingLayer, kMovingLayer);
    m_objectLayerPairFilter->EnableCollision(kMovingLayer, kNonMovingLayer);

    m_objectVsBroadPhaseFilter = std::make_unique<JPH::ObjectVsBroadPhaseLayerFilterTable>(
        *m_bpLayerInterface,
        BroadPhaseLayers::NUM_LAYERS,
        *m_objectLayerPairFilter,
        kNumObjectLayers
    );

    m_physics->Init(
        cMaxBodies,
        cNumBodyMutexes,
        cMaxBodyPairs,
        cMaxContactConstraints,
        *m_bpLayerInterface,
        *m_objectVsBroadPhaseFilter,
        *m_objectLayerPairFilter
    );

    m_initialized = true;
    log("PhysicsSystem initialized (Jolt).");
    return true;
}

void PhysicsSystem::shutdown() {
    if (!m_initialized) {
        return;
    }

    if (m_physics) {
        JPH::BodyInterface& bodies = m_physics->GetBodyInterface();

        for (const auto& [_, state] : m_entityBodies) {
            if (bodies.IsAdded(state.bodyId)) {
                bodies.RemoveBody(state.bodyId);
            }
            bodies.DestroyBody(state.bodyId);
        }

        for (const auto& bodyId : m_tileBodies) {
            if (bodies.IsAdded(bodyId)) {
                bodies.RemoveBody(bodyId);
            }
            bodies.DestroyBody(bodyId);
        }
    }

    m_entityBodies.clear();
    m_tileBodies.clear();
    m_bodyHalfExtents.clear();

    m_physics.reset();
    m_objectVsBroadPhaseFilter.reset();
    m_objectLayerPairFilter.reset();
    m_bpLayerInterface.reset();
    m_jobSystem.reset();
    m_tempAllocator.reset();

    JPH::UnregisterTypes();
    delete JPH::Factory::sInstance;
    JPH::Factory::sInstance = nullptr;

    m_initialized = false;
}

void PhysicsSystem::log(const std::string& line) const {
    if (m_logger) {
        m_logger(line);
    } else {
        spdlog::info("{}", line);
    }
}

glm::vec3 PhysicsSystem::defaultHalfExtentsFor(const SceneEntity& entity) const {
    const glm::vec3 s = entity.transform.scale;
    return glm::max(glm::vec3(0.15f), glm::abs(s) * 0.5f);
}

void PhysicsSystem::syncScene(Scene& scene) {
    if (!m_initialized || !m_physics) {
        return;
    }

    JPH::BodyInterface& bodies = m_physics->GetBodyInterface();
    std::unordered_set<SceneEntityId> alive;
    alive.reserve(scene.entities().size());

    for (auto& entity : scene.entities()) {
        if (!entity.physics.has_value() || !entity.physics->enabled) {
            continue;
        }

        alive.insert(entity.id);

        const bool dynamic = entity.physics->dynamic;
        glm::vec3 halfExtents = entity.physics->halfExtents;
        if (halfExtents.x <= 0.0f || halfExtents.y <= 0.0f || halfExtents.z <= 0.0f) {
            halfExtents = defaultHalfExtentsFor(entity);
        }

        auto it = m_entityBodies.find(entity.id);
        if (it == m_entityBodies.end()) {
            const JPH::Ref<JPH::BoxShape> shape = new JPH::BoxShape(JPH::Vec3(halfExtents.x, halfExtents.y, halfExtents.z));
            const float yawRad = glm::radians(entity.transform.rotationEulerDeg.y);
            JPH::Quat rot = JPH::Quat::sRotation(JPH::Vec3::sAxisY(), yawRad);

            JPH::BodyCreationSettings settings(
                shape,
                toJolt(entity.transform.position),
                rot,
                dynamic ? JPH::EMotionType::Dynamic : JPH::EMotionType::Kinematic,
                dynamic ? kMovingLayer : kNonMovingLayer
            );
            settings.mRestitution = 0.1f;
            settings.mFriction = 0.8f;

            const JPH::BodyID bodyId = bodies.CreateAndAddBody(settings, JPH::EActivation::Activate);
            m_entityBodies.emplace(entity.id, EntityBodyState{bodyId, dynamic});
            m_bodyHalfExtents[bodyId.GetIndexAndSequenceNumber()] = halfExtents;
            continue;
        }

        m_bodyHalfExtents[it->second.bodyId.GetIndexAndSequenceNumber()] = halfExtents;

        if (!it->second.dynamic) {
            const float yawRad = glm::radians(entity.transform.rotationEulerDeg.y);
            JPH::Quat rot = JPH::Quat::sRotation(JPH::Vec3::sAxisY(), yawRad);
            bodies.SetPositionAndRotation(it->second.bodyId, toJolt(entity.transform.position), rot, JPH::EActivation::DontActivate);
        }
    }

    for (auto it = m_entityBodies.begin(); it != m_entityBodies.end();) {
        if (!alive.contains(it->first)) {
            if (bodies.IsAdded(it->second.bodyId)) {
                bodies.RemoveBody(it->second.bodyId);
            }
            m_bodyHalfExtents.erase(it->second.bodyId.GetIndexAndSequenceNumber());
            bodies.DestroyBody(it->second.bodyId);
            it = m_entityBodies.erase(it);
        } else {
            ++it;
        }
    }
}

void PhysicsSystem::rebuildStaticTiles(const TileMap& tileMap) {
    if (!m_initialized || !m_physics) {
        return;
    }

    JPH::BodyInterface& bodies = m_physics->GetBodyInterface();
    for (const JPH::BodyID bodyId : m_tileBodies) {
        if (bodies.IsAdded(bodyId)) {
            bodies.RemoveBody(bodyId);
        }
        m_bodyHalfExtents.erase(bodyId.GetIndexAndSequenceNumber());
        bodies.DestroyBody(bodyId);
    }
    m_tileBodies.clear();

    const float half = tileMap.tileSize() * 0.5f;
    const glm::vec3 halfExtents{half, 0.1f, half};

    for (const auto& [coord, _tile] : tileMap.tiles()) {
        const glm::vec3 center = tileMap.cellToWorldCenter(coord.x, coord.z) + glm::vec3(0.0f, 0.1f, 0.0f);
        const JPH::Ref<JPH::BoxShape> shape = new JPH::BoxShape(JPH::Vec3(halfExtents.x, halfExtents.y, halfExtents.z));

        JPH::BodyCreationSettings settings(
            shape,
            toJolt(center),
            JPH::Quat::sIdentity(),
            JPH::EMotionType::Static,
            kNonMovingLayer
        );

        const JPH::BodyID bodyId = bodies.CreateAndAddBody(settings, JPH::EActivation::DontActivate);
        m_tileBodies.push_back(bodyId);
        m_bodyHalfExtents[bodyId.GetIndexAndSequenceNumber()] = halfExtents;
    }
}

void PhysicsSystem::step(Scene& scene, float dt) {
    if (!m_initialized || !m_physics) {
        return;
    }

    const bool doUpdate = m_simulationEnabled || m_singleStepRequested;
    if (doUpdate) {
        m_physics->Update(dt, 1, m_tempAllocator.get(), m_jobSystem.get());
        m_singleStepRequested = false;
    }

    if (!doUpdate) {
        return;
    }

    JPH::BodyInterface& bodies = m_physics->GetBodyInterface();
    for (auto& entity : scene.entities()) {
        if (!entity.physics.has_value() || !entity.physics->enabled || !entity.physics->dynamic) {
            continue;
        }

        auto it = m_entityBodies.find(entity.id);
        if (it == m_entityBodies.end()) {
            continue;
        }

        const JPH::RVec3 pos = bodies.GetCenterOfMassPosition(it->second.bodyId);
        const JPH::Quat rot = bodies.GetRotation(it->second.bodyId);

        entity.transform.position = toGlm(pos);

        const float w = rot.GetW();
        const float x = rot.GetX();
        const float y = rot.GetY();
        const float z = rot.GetZ();
        const float siny = 2.0f * (w * y + x * z);
        const float cosy = 1.0f - 2.0f * (y * y + z * z);
        entity.transform.rotationEulerDeg.y = glm::degrees(std::atan2(siny, cosy));
    }
}

void PhysicsSystem::addBoxLines(std::vector<glm::vec3>& lines, const JPH::BodyID& bodyId) const {
    if (!m_physics) {
        return;
    }

    const auto extIt = m_bodyHalfExtents.find(bodyId.GetIndexAndSequenceNumber());
    if (extIt == m_bodyHalfExtents.end()) {
        return;
    }

    const glm::vec3 e = extIt->second;
    const JPH::BodyInterface& bodies = m_physics->GetBodyInterface();
    const glm::vec3 c = toGlm(bodies.GetCenterOfMassPosition(bodyId));
    const JPH::Quat q = bodies.GetRotation(bodyId);

    std::array<glm::vec3, 8> corners = {
        glm::vec3(-e.x, -e.y, -e.z), glm::vec3(e.x, -e.y, -e.z),
        glm::vec3(e.x, e.y, -e.z), glm::vec3(-e.x, e.y, -e.z),
        glm::vec3(-e.x, -e.y, e.z), glm::vec3(e.x, -e.y, e.z),
        glm::vec3(e.x, e.y, e.z), glm::vec3(-e.x, e.y, e.z)
    };

    for (glm::vec3& corner : corners) {
        corner = c + rotateByQuat(q, corner);
    }

    constexpr std::array<std::pair<int, int>, 12> edges = {
        std::pair<int, int>{0, 1}, {1, 2}, {2, 3}, {3, 0},
        {4, 5}, {5, 6}, {6, 7}, {7, 4},
        {0, 4}, {1, 5}, {2, 6}, {3, 7}
    };

    for (const auto& [a, b] : edges) {
        lines.push_back(corners[static_cast<std::size_t>(a)]);
        lines.push_back(corners[static_cast<std::size_t>(b)]);
    }
}

std::vector<glm::vec3> PhysicsSystem::buildDebugLines() const {
    std::vector<glm::vec3> lines;
    lines.reserve((m_tileBodies.size() + m_entityBodies.size()) * 24);

    for (const auto& bodyId : m_tileBodies) {
        addBoxLines(lines, bodyId);
    }
    for (const auto& [_, state] : m_entityBodies) {
        addBoxLines(lines, state.bodyId);
    }

    return lines;
}

std::size_t PhysicsSystem::bodyCount() const {
    return m_tileBodies.size() + m_entityBodies.size();
}

} // namespace elysium
