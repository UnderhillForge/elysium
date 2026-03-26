#pragma once

#include "Procedural/ProceduralTileGenerator.hpp"

#include <glm/glm.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace elysium {

struct ProceduralVertex {
    glm::vec3 position{0.0f};
    glm::vec3 normal{0.0f, 1.0f, 0.0f};
    glm::vec2 uv{0.0f};
    glm::vec4 color{1.0f};
};

struct ProceduralDetailInstance {
    std::string id;
    glm::vec3 position{0.0f};
    float yawDeg{0.0f};
    float scale{1.0f};
};

struct ProceduralTileMesh {
    std::vector<ProceduralVertex> vertices;
    std::vector<std::uint32_t> indices;
    std::vector<ProceduralDetailInstance> details;

    std::string materialName;
    glm::vec3 localScale{1.0f};
    float localRotationDeg{0.0f};
};

struct TileMeshCacheKey {
    TileType baseType{TileType::Unknown};
    std::uint8_t neighborSignature{0};
    std::uint32_t seed{0};
    std::uint16_t variationBucket{0};

    bool operator==(const TileMeshCacheKey& rhs) const {
        return baseType == rhs.baseType &&
               neighborSignature == rhs.neighborSignature &&
               seed == rhs.seed &&
               variationBucket == rhs.variationBucket;
    }
};

struct TileMeshCacheKeyHash {
    std::size_t operator()(const TileMeshCacheKey& k) const {
        std::size_t h = static_cast<std::size_t>(k.baseType);
        h = (h * 1315423911u) ^ static_cast<std::size_t>(k.neighborSignature);
        h = (h * 2654435761u) ^ static_cast<std::size_t>(k.seed);
        h = (h * 2246822519u) ^ static_cast<std::size_t>(k.variationBucket);
        return h;
    }
};

class TileSetGenerator {
public:
    explicit TileSetGenerator(ProceduralTileConfig config = ProceduralTileConfig::defaults())
        : m_config(std::move(config)) {}

    const ProceduralTileConfig& config() const { return m_config; }
    ProceduralTileConfig& config() { return m_config; }

    std::shared_ptr<ProceduralTileMesh> buildTileMesh(
        const GeneratedTile& tile,
        float tileSize,
        std::uint32_t seed,
        float transitionIntensity);

    bool exportTileMeshToGlb(const ProceduralTileMesh& mesh,
                             const std::string& outputPath,
                             std::string& outError) const;

    void clearCache();
    std::size_t cacheSize() const { return m_cache.size(); }

private:
    std::uint16_t computeVariationBucket(const GeneratedTile& tile) const;
    void scatterDetails(const GeneratedTile& tile,
                        float tileSize,
                        std::uint32_t seed,
                        ProceduralTileMesh& mesh) const;

private:
    ProceduralTileConfig m_config;
    std::unordered_map<TileMeshCacheKey, std::shared_ptr<ProceduralTileMesh>, TileMeshCacheKeyHash> m_cache;
};

} // namespace elysium
