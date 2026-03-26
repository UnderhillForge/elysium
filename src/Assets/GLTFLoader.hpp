#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>

#include <filesystem>
#include <memory>
#include <vector>

namespace elysium {

struct GLTFMaterial {
    glm::vec4 baseColorFactor{1.0f};
    GLuint baseColorTexture{0};
    bool hasBaseColorTexture{false};
    GLuint normalTexture{0};
    bool hasNormalTexture{false};
    GLuint metallicRoughnessTexture{0};
    bool hasMetallicRoughnessTexture{false};
    float roughnessFactor{1.0f};
    float metallicFactor{0.0f};
    GLuint emissiveTexture{0};
    bool hasEmissiveTexture{false};
    glm::vec3 emissiveFactor{0.0f};
};

struct GLTFPrimitive {
    GLuint vao{0};
    GLuint vbo{0};
    GLuint ebo{0};
    GLsizei indexCount{0};
    GLsizei vertexCount{0};
    int materialIndex{-1};
    bool indexed{false};
};

class GLTFModel {
public:
    ~GLTFModel();

    std::vector<GLTFPrimitive> primitives;
    std::vector<GLTFMaterial> materials;
    std::vector<GLuint> textures;

    void destroy();
    void draw(GLuint shaderProgram, const glm::mat4& model, const glm::vec4& tint) const;
};

class GLTFLoader {
public:
    std::shared_ptr<GLTFModel> loadModelFromFile(const std::filesystem::path& filePath);
};

} // namespace elysium
