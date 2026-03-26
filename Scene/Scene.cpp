#include "Scene/Scene.hpp"

#include <algorithm>
#include <cstring>
#include <glm/gtc/matrix_transform.hpp>

namespace elysium {

glm::mat4 Transform::localMatrix() const {
    glm::mat4 m{1.0f};
    m = glm::translate(m, position);
    m = glm::rotate(m, glm::radians(rotationEulerDeg.x), glm::vec3(1, 0, 0));
    m = glm::rotate(m, glm::radians(rotationEulerDeg.y), glm::vec3(0, 1, 0));
    m = glm::rotate(m, glm::radians(rotationEulerDeg.z), glm::vec3(0, 0, 1));
    m = glm::scale(m, scale);
    return m;
}

void SceneEntity::syncNameBuffer() {
    nameBuffer.fill('\0');
    std::strncpy(nameBuffer.data(), name.c_str(), nameBuffer.size() - 1);
}

SceneEntityId Scene::createEntity(const std::string& name, const std::shared_ptr<GLTFModel>& model) {
    SceneEntity entity{};
    entity.id = m_nextEntityId++;
    entity.name = name.empty() ? "Entity" : name;
    entity.model = model;
    entity.syncNameBuffer();
    m_entities.push_back(entity);
    return entity.id;
}

bool Scene::destroyEntity(SceneEntityId id) {
    const auto it = std::find_if(m_entities.begin(), m_entities.end(), [id](const SceneEntity& entity) {
        return entity.id == id;
    });
    if (it == m_entities.end()) {
        return false;
    }
    m_entities.erase(it);
    return true;
}

void Scene::clear() {
    m_entities.clear();
    m_nextEntityId = 1;
}

SceneEntity* Scene::findEntity(SceneEntityId id) {
    auto it = std::find_if(m_entities.begin(), m_entities.end(), [id](const SceneEntity& entity) {
        return entity.id == id;
    });
    return it != m_entities.end() ? &(*it) : nullptr;
}

const SceneEntity* Scene::findEntity(SceneEntityId id) const {
    auto it = std::find_if(m_entities.begin(), m_entities.end(), [id](const SceneEntity& entity) {
        return entity.id == id;
    });
    return it != m_entities.end() ? &(*it) : nullptr;
}

void Scene::draw(GLuint shaderProgram) const {
    for (const auto& entity : m_entities) {
        if (!entity.model) {
            continue;
        }
        entity.model->draw(shaderProgram, entity.transform.localMatrix(), entity.tint);
    }
}

} // namespace elysium
