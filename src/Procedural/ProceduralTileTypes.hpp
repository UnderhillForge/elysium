#pragma once

#include <glm/glm.hpp>

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace elysium {

enum class TileType : std::uint8_t {
    Grass,
    Cobblestone,
    Sand,
    Rocky,
    Beach,
    Snow,
    Swamp,
    Water,
    Unknown
};

const char* tileTypeName(TileType type);
TileType tileTypeFromString(const std::string& value);

struct TileTypeHash {
    std::size_t operator()(TileType type) const {
        return static_cast<std::size_t>(type);
    }
};

struct EdgeDisplacementProfile {
    // Lower frequencies create broad coast/terrain undulation, high frequencies add micro breakup.
    float lowFrequency{0.90f};
    float highFrequency{5.20f};
    float lowAmplitude{0.075f};
    float highAmplitude{0.030f};

    // Vertical noise subtly breaks flat planes and helps blend transitions.
    float verticalAmplitude{0.020f};

    // Controls how quickly displacement ramps as vertices approach tile edges.
    float edgeFalloff{1.40f};

    // Extra boost when neighboring tiles are different biome types.
    float transitionBoost{0.40f};
};

struct DetailVariant {
    std::string id;
    float weight{1.0f};
    float minScale{0.85f};
    float maxScale{1.15f};
};

struct TileTypeProperties {
    TileType type{TileType::Unknown};
    std::string materialName;
    glm::vec4 baseTint{1.0f};
    bool walkable{true};

    // Rotation/scale jitter are applied per tile instance for visual de-tiling.
    float maxRotationJitterDeg{4.0f};
    float maxUniformScaleJitter{0.03f};

    float detailDensity{0.0f};
    std::vector<DetailVariant> detailVariants;
    EdgeDisplacementProfile edge;
};

class ProceduralTileConfig {
public:
    static ProceduralTileConfig defaults();
    static bool loadFromJsonFile(const std::string& path, ProceduralTileConfig& outConfig, std::string& outError);

    const TileTypeProperties& properties(TileType type) const;
    bool hasType(TileType type) const;

    std::unordered_map<TileType, TileTypeProperties, TileTypeHash> typeProperties;
};

} // namespace elysium
