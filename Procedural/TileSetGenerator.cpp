#include "Procedural/TileSetGenerator.hpp"

#include <tiny_gltf.h>

#include <glm/geometric.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace elysium {

namespace {

constexpr std::uint8_t kBitN = 1u << 0u;
constexpr std::uint8_t kBitE = 1u << 2u;
constexpr std::uint8_t kBitS = 1u << 4u;
constexpr std::uint8_t kBitW = 1u << 6u;

std::uint32_t hashCoords(std::int32_t x, std::int32_t z, std::uint32_t seed) {
    std::uint32_t h = seed;
    h ^= static_cast<std::uint32_t>(x) * 0x85ebca6bu;
    h = (h << 13u) | (h >> 19u);
    h ^= static_cast<std::uint32_t>(z) * 0xc2b2ae35u;
    h ^= (h >> 16u);
    h *= 0x7feb352du;
    h ^= (h >> 15u);
    h *= 0x846ca68bu;
    h ^= (h >> 16u);
    return h;
}

float random01(std::int32_t x, std::int32_t z, std::uint32_t seed) {
    const std::uint32_t h = hashCoords(x, z, seed);
    return static_cast<float>(h & 0x00FFFFFFu) / static_cast<float>(0x01000000u);
}

float remapSigned(float v01) {
    return v01 * 2.0f - 1.0f;
}

float smoothstep(float t) {
    return t * t * (3.0f - 2.0f * t);
}

float valueNoise2D(float x, float z, std::uint32_t seed) {
    const int x0 = static_cast<int>(std::floor(x));
    const int z0 = static_cast<int>(std::floor(z));
    const int x1 = x0 + 1;
    const int z1 = z0 + 1;

    const float fx = x - static_cast<float>(x0);
    const float fz = z - static_cast<float>(z0);

    const float n00 = random01(x0, z0, seed);
    const float n10 = random01(x1, z0, seed);
    const float n01 = random01(x0, z1, seed);
    const float n11 = random01(x1, z1, seed);

    const float ux = smoothstep(fx);
    const float uz = smoothstep(fz);

    const float nx0 = n00 + (n10 - n00) * ux;
    const float nx1 = n01 + (n11 - n01) * ux;
    return nx0 + (nx1 - nx0) * uz;
}

float clamp01(float v) {
    return std::clamp(v, 0.0f, 1.0f);
}

float edgeWeight(float value, float halfTile, float falloffPower) {
    const float d = std::abs(value) / std::max(halfTile, 0.0001f);
    return std::pow(clamp01(d), std::max(0.1f, falloffPower));
}

void addTriangle(std::vector<std::uint32_t>& indices, std::uint32_t a, std::uint32_t b, std::uint32_t c) {
    indices.push_back(a);
    indices.push_back(b);
    indices.push_back(c);
}

glm::vec4 blendTransitionColor(const glm::vec4& base,
                               std::uint8_t neighborSig,
                               float edgeNorth,
                               float edgeEast,
                               float edgeSouth,
                               float edgeWest,
                               float transitionIntensity)
{
    float transition = 0.0f;
    if ((neighborSig & kBitN) == 0u) transition = std::max(transition, edgeNorth);
    if ((neighborSig & kBitE) == 0u) transition = std::max(transition, edgeEast);
    if ((neighborSig & kBitS) == 0u) transition = std::max(transition, edgeSouth);
    if ((neighborSig & kBitW) == 0u) transition = std::max(transition, edgeWest);

    const float blend = clamp01(transition * transitionIntensity);
    const glm::vec3 lift = glm::vec3(base) + glm::vec3(0.08f, 0.08f, 0.08f) * blend;
    return glm::vec4(clamp01(lift.r), clamp01(lift.g), clamp01(lift.b), base.a);
}

} // namespace

std::shared_ptr<ProceduralTileMesh> TileSetGenerator::buildTileMesh(const GeneratedTile& tile,
                                                                    float tileSize,
                                                                    std::uint32_t seed,
                                                                    float transitionIntensity)
{
    const TileMeshCacheKey cacheKey{
        tile.type,
        tile.neighborSignature,
        seed,
        computeVariationBucket(tile),
    };

    auto cached = m_cache.find(cacheKey);
    if (cached != m_cache.end()) {
        return cached->second;
    }

    const TileTypeProperties& props = m_config.properties(tile.type);
    auto mesh = std::make_shared<ProceduralTileMesh>();
    mesh->materialName = props.materialName;
    mesh->localRotationDeg = tile.rotationJitterDeg;
    mesh->localScale = glm::vec3(tile.scaleJitter);

    // Dense enough lattice to make edge displacement organic without becoming too heavy.
    constexpr int kGridRes = 8;
    const float half = tileSize * 0.5f;
    const int vertCols = kGridRes + 1;

    mesh->vertices.reserve(static_cast<std::size_t>(vertCols * vertCols));
    mesh->indices.reserve(static_cast<std::size_t>(kGridRes * kGridRes * 6));

    for (int row = 0; row <= kGridRes; ++row) {
        const float v = static_cast<float>(row) / static_cast<float>(kGridRes);
        const float zBase = (v - 0.5f) * tileSize;

        for (int col = 0; col <= kGridRes; ++col) {
            const float u = static_cast<float>(col) / static_cast<float>(kGridRes);
            const float xBase = (u - 0.5f) * tileSize;

            const float edgeN = clamp01((-zBase + half) / tileSize);
            const float edgeS = clamp01((zBase + half) / tileSize);
            const float edgeW = clamp01((-xBase + half) / tileSize);
            const float edgeE = clamp01((xBase + half) / tileSize);

            const float edgeProximity = std::max(edgeWeight(xBase, half, props.edge.edgeFalloff),
                                                 edgeWeight(zBase, half, props.edge.edgeFalloff));

            float transitionEdge = 0.0f;
            if ((tile.neighborSignature & kBitN) == 0u) transitionEdge = std::max(transitionEdge, edgeN);
            if ((tile.neighborSignature & kBitE) == 0u) transitionEdge = std::max(transitionEdge, edgeE);
            if ((tile.neighborSignature & kBitS) == 0u) transitionEdge = std::max(transitionEdge, edgeS);
            if ((tile.neighborSignature & kBitW) == 0u) transitionEdge = std::max(transitionEdge, edgeW);

            const float low = remapSigned(valueNoise2D(
                (xBase + static_cast<float>(tile.x) * tileSize) * props.edge.lowFrequency,
                (zBase + static_cast<float>(tile.z) * tileSize) * props.edge.lowFrequency,
                seed ^ 0xA2C79D41u));
            const float high = remapSigned(valueNoise2D(
                (xBase + static_cast<float>(tile.x) * tileSize) * props.edge.highFrequency,
                (zBase + static_cast<float>(tile.z) * tileSize) * props.edge.highFrequency,
                seed ^ 0x6A5D39E1u));

            const float displacementBase = low * props.edge.lowAmplitude + high * props.edge.highAmplitude;
            const float amplitude = (0.20f + edgeProximity) * (1.0f + transitionEdge * props.edge.transitionBoost * transitionIntensity);
            const float planarDisplacement = displacementBase * amplitude;
            const float verticalDisplacement = (low * 0.65f + high * 0.35f) * props.edge.verticalAmplitude * amplitude;

            glm::vec2 outDir = glm::vec2(xBase, zBase);
            if (glm::dot(outDir, outDir) < 1e-6f) {
                outDir = glm::vec2(1.0f, 0.0f);
            } else {
                outDir = glm::normalize(outDir);
            }

            const float x = xBase + outDir.x * planarDisplacement;
            const float z = zBase + outDir.y * planarDisplacement;
            const float y = verticalDisplacement;

            ProceduralVertex vertex{};
            vertex.position = glm::vec3(x, y, z);
            vertex.uv = glm::vec2(u, v);
            vertex.color = blendTransitionColor(props.baseTint,
                                                tile.neighborSignature,
                                                edgeN,
                                                edgeE,
                                                edgeS,
                                                edgeW,
                                                transitionIntensity);
            mesh->vertices.push_back(vertex);
        }
    }

    for (int row = 0; row < kGridRes; ++row) {
        for (int col = 0; col < kGridRes; ++col) {
            const std::uint32_t i0 = static_cast<std::uint32_t>(row * vertCols + col);
            const std::uint32_t i1 = i0 + 1u;
            const std::uint32_t i2 = i0 + static_cast<std::uint32_t>(vertCols);
            const std::uint32_t i3 = i2 + 1u;

            addTriangle(mesh->indices, i0, i2, i1);
            addTriangle(mesh->indices, i1, i2, i3);
        }
    }

    // Recompute smooth normals after edge displacement to avoid shading artifacts.
    for (auto& vertex : mesh->vertices) {
        vertex.normal = glm::vec3(0.0f);
    }
    for (std::size_t i = 0; i + 2 < mesh->indices.size(); i += 3) {
        const std::uint32_t ia = mesh->indices[i + 0];
        const std::uint32_t ib = mesh->indices[i + 1];
        const std::uint32_t ic = mesh->indices[i + 2];
        const glm::vec3 a = mesh->vertices[ia].position;
        const glm::vec3 b = mesh->vertices[ib].position;
        const glm::vec3 c = mesh->vertices[ic].position;
        const glm::vec3 n = glm::cross(b - a, c - a);
        mesh->vertices[ia].normal += n;
        mesh->vertices[ib].normal += n;
        mesh->vertices[ic].normal += n;
    }
    for (auto& vertex : mesh->vertices) {
        if (glm::dot(vertex.normal, vertex.normal) < 1e-8f) {
            vertex.normal = glm::vec3(0.0f, 1.0f, 0.0f);
        } else {
            vertex.normal = glm::normalize(vertex.normal);
        }
    }

    scatterDetails(tile, tileSize, seed, *mesh);
    m_cache.emplace(cacheKey, mesh);
    return mesh;
}

bool TileSetGenerator::exportTileMeshToGlb(const ProceduralTileMesh& mesh,
                                           const std::string& outputPath,
                                           std::string& outError) const
{
    tinygltf::Model model;
    model.asset.version = "2.0";
    model.asset.generator = "Elysium TileSetGenerator";

    std::vector<float> positions;
    std::vector<float> normals;
    std::vector<float> uvs;
    std::vector<float> colors;
    positions.reserve(mesh.vertices.size() * 3u);
    normals.reserve(mesh.vertices.size() * 3u);
    uvs.reserve(mesh.vertices.size() * 2u);
    colors.reserve(mesh.vertices.size() * 4u);

    for (const auto& v : mesh.vertices) {
        positions.push_back(v.position.x);
        positions.push_back(v.position.y);
        positions.push_back(v.position.z);

        normals.push_back(v.normal.x);
        normals.push_back(v.normal.y);
        normals.push_back(v.normal.z);

        uvs.push_back(v.uv.x);
        uvs.push_back(v.uv.y);

        colors.push_back(v.color.r);
        colors.push_back(v.color.g);
        colors.push_back(v.color.b);
        colors.push_back(v.color.a);
    }

    std::vector<std::uint32_t> indices = mesh.indices;

    tinygltf::Buffer buffer;
    buffer.data.reserve(
        positions.size() * sizeof(float) +
        normals.size() * sizeof(float) +
        uvs.size() * sizeof(float) +
        colors.size() * sizeof(float) +
        indices.size() * sizeof(std::uint32_t));

    auto append = [&](const void* data, std::size_t bytes) -> std::size_t {
        const std::size_t offset = buffer.data.size();
        const auto* p = static_cast<const unsigned char*>(data);
        buffer.data.insert(buffer.data.end(), p, p + bytes);
        return offset;
    };

    const std::size_t posOffset = append(positions.data(), positions.size() * sizeof(float));
    const std::size_t nrmOffset = append(normals.data(), normals.size() * sizeof(float));
    const std::size_t uvOffset = append(uvs.data(), uvs.size() * sizeof(float));
    const std::size_t colOffset = append(colors.data(), colors.size() * sizeof(float));
    const std::size_t idxOffset = append(indices.data(), indices.size() * sizeof(std::uint32_t));

    model.buffers.push_back(std::move(buffer));

    auto makeView = [&](std::size_t offset, std::size_t bytes, int target) {
        tinygltf::BufferView view;
        view.buffer = 0;
        view.byteOffset = offset;
        view.byteLength = bytes;
        view.target = target;
        model.bufferViews.push_back(view);
        return static_cast<int>(model.bufferViews.size() - 1);
    };

    const int posView = makeView(posOffset, positions.size() * sizeof(float), TINYGLTF_TARGET_ARRAY_BUFFER);
    const int nrmView = makeView(nrmOffset, normals.size() * sizeof(float), TINYGLTF_TARGET_ARRAY_BUFFER);
    const int uvView = makeView(uvOffset, uvs.size() * sizeof(float), TINYGLTF_TARGET_ARRAY_BUFFER);
    const int colView = makeView(colOffset, colors.size() * sizeof(float), TINYGLTF_TARGET_ARRAY_BUFFER);
    const int idxView = makeView(idxOffset, indices.size() * sizeof(std::uint32_t), TINYGLTF_TARGET_ELEMENT_ARRAY_BUFFER);

    auto makeAccessor = [&](int view, int componentType, int type, std::size_t count) {
        tinygltf::Accessor accessor;
        accessor.bufferView = view;
        accessor.componentType = componentType;
        accessor.type = type;
        accessor.count = count;
        model.accessors.push_back(accessor);
        return static_cast<int>(model.accessors.size() - 1);
    };

    const int posAccessor = makeAccessor(posView, TINYGLTF_COMPONENT_TYPE_FLOAT, TINYGLTF_TYPE_VEC3, mesh.vertices.size());
    const int nrmAccessor = makeAccessor(nrmView, TINYGLTF_COMPONENT_TYPE_FLOAT, TINYGLTF_TYPE_VEC3, mesh.vertices.size());
    const int uvAccessor = makeAccessor(uvView, TINYGLTF_COMPONENT_TYPE_FLOAT, TINYGLTF_TYPE_VEC2, mesh.vertices.size());
    const int colAccessor = makeAccessor(colView, TINYGLTF_COMPONENT_TYPE_FLOAT, TINYGLTF_TYPE_VEC4, mesh.vertices.size());
    const int idxAccessor = makeAccessor(idxView, TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT, TINYGLTF_TYPE_SCALAR, mesh.indices.size());

    tinygltf::Primitive primitive;
    primitive.mode = TINYGLTF_MODE_TRIANGLES;
    primitive.indices = idxAccessor;
    primitive.attributes["POSITION"] = posAccessor;
    primitive.attributes["NORMAL"] = nrmAccessor;
    primitive.attributes["TEXCOORD_0"] = uvAccessor;
    primitive.attributes["COLOR_0"] = colAccessor;

    tinygltf::Mesh gltfMesh;
    gltfMesh.name = "tile_mesh";
    gltfMesh.primitives.push_back(std::move(primitive));
    model.meshes.push_back(std::move(gltfMesh));

    tinygltf::Node node;
    node.mesh = 0;
    node.name = "tile_root";
    node.rotation = {
        0.0,
        std::sin(mesh.localRotationDeg * 3.14159265358979323846 / 360.0),
        0.0,
        std::cos(mesh.localRotationDeg * 3.14159265358979323846 / 360.0)
    };
    node.scale = {
        static_cast<double>(mesh.localScale.x),
        static_cast<double>(mesh.localScale.y),
        static_cast<double>(mesh.localScale.z)
    };

    model.nodes.push_back(std::move(node));

    for (const auto& detail : mesh.details) {
        tinygltf::Node detailNode;
        detailNode.name = "detail_" + detail.id;
        detailNode.translation = {
            static_cast<double>(detail.position.x),
            static_cast<double>(detail.position.y),
            static_cast<double>(detail.position.z)
        };
        detailNode.rotation = {
            0.0,
            std::sin(detail.yawDeg * 3.14159265358979323846 / 360.0),
            0.0,
            std::cos(detail.yawDeg * 3.14159265358979323846 / 360.0)
        };
        detailNode.scale = {
            static_cast<double>(detail.scale),
            static_cast<double>(detail.scale),
            static_cast<double>(detail.scale)
        };
        model.nodes.push_back(std::move(detailNode));
        model.nodes[0].children.push_back(static_cast<int>(model.nodes.size() - 1));
    }

    tinygltf::Scene scene;
    scene.nodes.push_back(0);
    model.scenes.push_back(std::move(scene));
    model.defaultScene = 0;

    tinygltf::TinyGLTF writer;
    if (!writer.WriteGltfSceneToFile(&model, outputPath, true, true, true, true)) {
        outError = "Failed to write tile glb: " + outputPath;
        return false;
    }
    return true;
}

void TileSetGenerator::clearCache() {
    m_cache.clear();
}

std::uint16_t TileSetGenerator::computeVariationBucket(const GeneratedTile& tile) const {
    const float normalizedRot = (tile.rotationJitterDeg + 16.0f) / 32.0f;
    const float normalizedScale = (tile.scaleJitter - 0.90f) / 0.20f;
    const std::uint16_t rotBucket = static_cast<std::uint16_t>(std::clamp(normalizedRot, 0.0f, 0.999f) * 16.0f);
    const std::uint16_t scaleBucket = static_cast<std::uint16_t>(std::clamp(normalizedScale, 0.0f, 0.999f) * 16.0f);
    return static_cast<std::uint16_t>((rotBucket << 4u) | scaleBucket);
}

void TileSetGenerator::scatterDetails(const GeneratedTile& tile,
                                      float tileSize,
                                      std::uint32_t seed,
                                      ProceduralTileMesh& mesh) const
{
    const TileTypeProperties& props = m_config.properties(tile.type);
    if (props.detailDensity <= 0.0f || props.detailVariants.empty()) {
        return;
    }

    const int desired = static_cast<int>(std::round(props.detailDensity * 6.0f));
    const int detailCount = std::clamp(desired, 0, 16);
    const float half = tileSize * 0.5f;

    for (int i = 0; i < detailCount; ++i) {
        const float rx = random01(tile.x * 31 + i * 17, tile.z * 13 + i * 7, seed ^ 0x9e3779b9u);
        const float rz = random01(tile.x * 29 + i * 11, tile.z * 19 + i * 5, seed ^ 0x85ebca6bu);
        const float rw = random01(tile.x * 23 + i * 3, tile.z * 37 + i * 2, seed ^ 0xc2b2ae35u);

        float picker = rw;
        const DetailVariant* chosen = &props.detailVariants.front();
        float totalWeight = 0.0f;
        for (const auto& v : props.detailVariants) {
            totalWeight += std::max(0.0001f, v.weight);
        }
        float running = 0.0f;
        for (const auto& v : props.detailVariants) {
            running += std::max(0.0001f, v.weight) / totalWeight;
            if (picker <= running) {
                chosen = &v;
                break;
            }
        }

        ProceduralDetailInstance instance{};
        instance.id = chosen->id;
        instance.position = glm::vec3(
            -half + rx * tileSize,
            0.0f,
            -half + rz * tileSize);
        instance.yawDeg = random01(tile.x + i * 97, tile.z + i * 71, seed ^ 0x27d4eb2du) * 360.0f;
        const float s = random01(tile.x + i * 43, tile.z + i * 61, seed ^ 0x165667b1u);
        instance.scale = chosen->minScale + (chosen->maxScale - chosen->minScale) * s;
        mesh.details.push_back(instance);
    }
}

} // namespace elysium
