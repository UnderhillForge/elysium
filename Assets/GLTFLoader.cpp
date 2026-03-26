#include "Assets/GLTFLoader.hpp"

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <tiny_gltf.h>

#include <glm/gtc/type_ptr.hpp>
#include <spdlog/spdlog.h>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace elysium {

namespace {

struct Vertex {
    glm::vec3 pos{0.0f};
    glm::vec3 nrm{0.0f, 1.0f, 0.0f};
    glm::vec2 uv{0.0f};
};

static int bytesPerComponent(int componentType) {
    switch (componentType) {
        case TINYGLTF_COMPONENT_TYPE_BYTE:
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE: return 1;
        case TINYGLTF_COMPONENT_TYPE_SHORT:
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: return 2;
        case TINYGLTF_COMPONENT_TYPE_INT:
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
        case TINYGLTF_COMPONENT_TYPE_FLOAT: return 4;
        default: return 0;
    }
}

static int numComponents(int type) {
    switch (type) {
        case TINYGLTF_TYPE_SCALAR: return 1;
        case TINYGLTF_TYPE_VEC2: return 2;
        case TINYGLTF_TYPE_VEC3: return 3;
        case TINYGLTF_TYPE_VEC4: return 4;
        default: return 0;
    }
}

template <typename T>
static void copyAccessorToVec(const tinygltf::Model& model, int accessorIndex, std::vector<T>& out) {
    const tinygltf::Accessor& acc = model.accessors[static_cast<std::size_t>(accessorIndex)];
    const tinygltf::BufferView& bv = model.bufferViews[static_cast<std::size_t>(acc.bufferView)];
    const tinygltf::Buffer& b = model.buffers[static_cast<std::size_t>(bv.buffer)];

    const int compCount = numComponents(acc.type);
    const int stride = acc.ByteStride(bv);
    const int elemSize = bytesPerComponent(acc.componentType) * compCount;
    const std::size_t byteOffset = static_cast<std::size_t>(bv.byteOffset + acc.byteOffset);

    out.resize(acc.count);

    for (std::size_t i = 0; i < acc.count; ++i) {
        const std::size_t srcOff = byteOffset + i * static_cast<std::size_t>(stride ? stride : elemSize);
        const void* src = b.data.data() + srcOff;
        std::memcpy(&out[i], src, sizeof(T));
    }
}

static GLuint uploadTexture(const tinygltf::Image& img) {
    if (img.image.empty() || img.width <= 0 || img.height <= 0) {
        return 0;
    }

    GLenum fmt = GL_RGBA;
    if (img.component == 1) fmt = GL_RED;
    if (img.component == 2) fmt = GL_RG;
    if (img.component == 3) fmt = GL_RGB;
    if (img.component == 4) fmt = GL_RGBA;

    GLuint tex = 0;
    glCreateTextures(GL_TEXTURE_2D, 1, &tex);
    glTextureStorage2D(tex, 1, GL_RGBA8, img.width, img.height);
    glTextureSubImage2D(tex, 0, 0, 0, img.width, img.height, fmt, GL_UNSIGNED_BYTE, img.image.data());
    glGenerateTextureMipmap(tex);

    glTextureParameteri(tex, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTextureParameteri(tex, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTextureParameteri(tex, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTextureParameteri(tex, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    return tex;
}

} // namespace

GLTFModel::~GLTFModel() {
    destroy();
}

void GLTFModel::destroy() {
    for (auto& p : primitives) {
        if (p.ebo) glDeleteBuffers(1, &p.ebo);
        if (p.vbo) glDeleteBuffers(1, &p.vbo);
        if (p.vao) glDeleteVertexArrays(1, &p.vao);
        p = {};
    }
    for (GLuint t : textures) {
        glDeleteTextures(1, &t);
    }
    primitives.clear();
    materials.clear();
    textures.clear();
}

void GLTFModel::draw(GLuint shaderProgram, const glm::mat4& model, const glm::vec4& tint) const {
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "u_Model"), 1, GL_FALSE, glm::value_ptr(model));
    glUniform4fv(glGetUniformLocation(shaderProgram, "u_Tint"), 1, glm::value_ptr(tint));

    for (const auto& prim : primitives) {
        GLTFMaterial mat{};
        if (prim.materialIndex >= 0 && prim.materialIndex < static_cast<int>(materials.size())) {
            mat = materials[static_cast<std::size_t>(prim.materialIndex)];
        }

        glUniform4fv(glGetUniformLocation(shaderProgram, "u_BaseColorFactor"), 1, glm::value_ptr(mat.baseColorFactor));
        glUniform1i(glGetUniformLocation(shaderProgram, "u_HasBaseColorTexture"), mat.hasBaseColorTexture ? 1 : 0);
        if (mat.hasBaseColorTexture) {
            glBindTextureUnit(0, mat.baseColorTexture);
            glUniform1i(glGetUniformLocation(shaderProgram, "u_BaseColorTex"), 0);
        }

            glUniform1i(glGetUniformLocation(shaderProgram, "u_HasNormalTexture"), mat.hasNormalTexture ? 1 : 0);
            if (mat.hasNormalTexture) {
                glBindTextureUnit(1, mat.normalTexture);
                glUniform1i(glGetUniformLocation(shaderProgram, "u_NormalTex"), 1);
            }
            glUniform1i(glGetUniformLocation(shaderProgram, "u_HasMetallicRoughnessTex"), mat.hasMetallicRoughnessTexture ? 1 : 0);
            if (mat.hasMetallicRoughnessTexture) {
                glBindTextureUnit(2, mat.metallicRoughnessTexture);
                glUniform1i(glGetUniformLocation(shaderProgram, "u_MetallicRoughnessTex"), 2);
            }
            glUniform1f(glGetUniformLocation(shaderProgram, "u_Roughness"), mat.roughnessFactor);
            glUniform1f(glGetUniformLocation(shaderProgram, "u_Metallic"),  mat.metallicFactor);
            glUniform1i(glGetUniformLocation(shaderProgram, "u_HasEmissiveTex"), mat.hasEmissiveTexture ? 1 : 0);
            if (mat.hasEmissiveTexture) {
                glBindTextureUnit(3, mat.emissiveTexture);
                glUniform1i(glGetUniformLocation(shaderProgram, "u_EmissiveTex"), 3);
            }
            glUniform3fv(glGetUniformLocation(shaderProgram, "u_EmissiveFactor"), 1, glm::value_ptr(mat.emissiveFactor));

        glBindVertexArray(prim.vao);
        if (prim.indexed) {
            glDrawElements(GL_TRIANGLES, prim.indexCount, GL_UNSIGNED_INT, nullptr);
        } else {
            glDrawArrays(GL_TRIANGLES, 0, prim.vertexCount);
        }
    }

    glBindVertexArray(0);
}

std::shared_ptr<GLTFModel> GLTFLoader::loadModelFromFile(const std::filesystem::path& filePath) {
    tinygltf::TinyGLTF loader;
    tinygltf::Model src;
    std::string warn;
    std::string err;

    bool ok = false;
    const std::string pathStr = filePath.string();
    if (filePath.extension() == ".glb") {
        ok = loader.LoadBinaryFromFile(&src, &err, &warn, pathStr);
    } else {
        ok = loader.LoadASCIIFromFile(&src, &err, &warn, pathStr);
    }

    if (!warn.empty()) spdlog::warn("tinygltf warning: {}", warn);
    if (!err.empty()) spdlog::error("tinygltf error: {}", err);
    if (!ok) return nullptr;

    auto model = std::make_shared<GLTFModel>();

    for (const auto& img : src.images) {
        GLuint tex = uploadTexture(img);
        model->textures.push_back(tex);
    }

    model->materials.reserve(src.materials.size());
    for (const auto& m : src.materials) {
        GLTFMaterial out{};
        if (m.values.contains("baseColorFactor")) {
            const auto c = m.values.at("baseColorFactor").ColorFactor();
            out.baseColorFactor = glm::vec4(
                static_cast<float>(c[0]),
                static_cast<float>(c[1]),
                static_cast<float>(c[2]),
                static_cast<float>(c[3])
            );
        }
        if (m.values.contains("baseColorTexture")) {
            const int texIndex = m.values.at("baseColorTexture").TextureIndex();
            if (texIndex >= 0 && texIndex < static_cast<int>(src.textures.size())) {
                const int imageIndex = src.textures[static_cast<std::size_t>(texIndex)].source;
                if (imageIndex >= 0 && imageIndex < static_cast<int>(model->textures.size())) {
                    out.baseColorTexture = model->textures[static_cast<std::size_t>(imageIndex)];
                    out.hasBaseColorTexture = true;
                }
            }
        }

        // Normal map
        if (m.normalTexture.index >= 0 && m.normalTexture.index < static_cast<int>(src.textures.size())) {
            const int imgIdx = src.textures[static_cast<std::size_t>(m.normalTexture.index)].source;
            if (imgIdx >= 0 && imgIdx < static_cast<int>(model->textures.size()))
                out.normalTexture = model->textures[static_cast<std::size_t>(imgIdx)], out.hasNormalTexture = true;
        }

        // Emissive
        out.emissiveFactor = glm::vec3{
            static_cast<float>(m.emissiveFactor[0]),
            static_cast<float>(m.emissiveFactor[1]),
            static_cast<float>(m.emissiveFactor[2])};
        if (m.emissiveTexture.index >= 0 && m.emissiveTexture.index < static_cast<int>(src.textures.size())) {
            const int imgIdx = src.textures[static_cast<std::size_t>(m.emissiveTexture.index)].source;
            if (imgIdx >= 0 && imgIdx < static_cast<int>(model->textures.size()))
                out.emissiveTexture = model->textures[static_cast<std::size_t>(imgIdx)], out.hasEmissiveTexture = true;
        }

        // PBR metallic-roughness
        out.roughnessFactor = static_cast<float>(m.pbrMetallicRoughness.roughnessFactor);
        out.metallicFactor  = static_cast<float>(m.pbrMetallicRoughness.metallicFactor);
        {
            const int mrIdx = m.pbrMetallicRoughness.metallicRoughnessTexture.index;
            if (mrIdx >= 0 && mrIdx < static_cast<int>(src.textures.size())) {
                const int imgIdx = src.textures[static_cast<std::size_t>(mrIdx)].source;
                if (imgIdx >= 0 && imgIdx < static_cast<int>(model->textures.size()))
                    out.metallicRoughnessTexture = model->textures[static_cast<std::size_t>(imgIdx)],
                    out.hasMetallicRoughnessTexture = true;
            }
        }

        model->materials.push_back(out);
    }

    for (const auto& mesh : src.meshes) {
        for (const auto& prim : mesh.primitives) {
            if (prim.mode != TINYGLTF_MODE_TRIANGLES) {
                continue;
            }

            auto posIt = prim.attributes.find("POSITION");
            if (posIt == prim.attributes.end()) {
                continue;
            }

            std::vector<glm::vec3> positions;
            copyAccessorToVec(src, posIt->second, positions);

            std::vector<glm::vec3> normals;
            auto nrmIt = prim.attributes.find("NORMAL");
            if (nrmIt != prim.attributes.end()) {
                copyAccessorToVec(src, nrmIt->second, normals);
            }

            std::vector<glm::vec2> uvs;
            auto uvIt = prim.attributes.find("TEXCOORD_0");
            if (uvIt != prim.attributes.end()) {
                copyAccessorToVec(src, uvIt->second, uvs);
            }

            std::vector<Vertex> verts(positions.size());
            for (std::size_t i = 0; i < verts.size(); ++i) {
                verts[i].pos = positions[i];
                if (i < normals.size()) verts[i].nrm = normals[i];
                if (i < uvs.size()) verts[i].uv = uvs[i];
            }

            std::vector<uint32_t> indices;
            bool indexed = prim.indices >= 0;
            if (indexed) {
                const auto& acc = src.accessors[static_cast<std::size_t>(prim.indices)];
                const auto& bv = src.bufferViews[static_cast<std::size_t>(acc.bufferView)];
                const auto& b = src.buffers[static_cast<std::size_t>(bv.buffer)];
                const std::size_t off = static_cast<std::size_t>(bv.byteOffset + acc.byteOffset);

                indices.resize(acc.count);
                for (std::size_t i = 0; i < acc.count; ++i) {
                    if (acc.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
                        const auto* ptr = reinterpret_cast<const uint16_t*>(b.data.data() + off);
                        indices[i] = ptr[i];
                    } else if (acc.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
                        const auto* ptr = reinterpret_cast<const uint8_t*>(b.data.data() + off);
                        indices[i] = ptr[i];
                    } else if (acc.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
                        const auto* ptr = reinterpret_cast<const uint32_t*>(b.data.data() + off);
                        indices[i] = ptr[i];
                    }
                }
            }

            GLTFPrimitive out{};
            out.indexed = indexed;
            out.materialIndex = prim.material;
            out.vertexCount = static_cast<GLsizei>(verts.size());
            out.indexCount = static_cast<GLsizei>(indices.size());

            glCreateVertexArrays(1, &out.vao);
            glCreateBuffers(1, &out.vbo);
            glNamedBufferData(out.vbo, static_cast<GLsizeiptr>(verts.size() * sizeof(Vertex)), verts.data(), GL_STATIC_DRAW);

            glVertexArrayVertexBuffer(out.vao, 0, out.vbo, 0, sizeof(Vertex));
            glEnableVertexArrayAttrib(out.vao, 0);
            glEnableVertexArrayAttrib(out.vao, 1);
            glEnableVertexArrayAttrib(out.vao, 2);
            glVertexArrayAttribFormat(out.vao, 0, 3, GL_FLOAT, GL_FALSE, offsetof(Vertex, pos));
            glVertexArrayAttribFormat(out.vao, 1, 3, GL_FLOAT, GL_FALSE, offsetof(Vertex, nrm));
            glVertexArrayAttribFormat(out.vao, 2, 2, GL_FLOAT, GL_FALSE, offsetof(Vertex, uv));
            glVertexArrayAttribBinding(out.vao, 0, 0);
            glVertexArrayAttribBinding(out.vao, 1, 0);
            glVertexArrayAttribBinding(out.vao, 2, 0);

            if (indexed) {
                glCreateBuffers(1, &out.ebo);
                glNamedBufferData(
                    out.ebo,
                    static_cast<GLsizeiptr>(indices.size() * sizeof(uint32_t)),
                    indices.data(),
                    GL_STATIC_DRAW
                );
                glVertexArrayElementBuffer(out.vao, out.ebo);
            }

            model->primitives.push_back(out);
        }
    }

    spdlog::info("Loaded glTF: {} (primitives: {})", filePath.string(), model->primitives.size());
    return model;
}

} // namespace elysium
