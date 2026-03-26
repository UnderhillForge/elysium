#pragma once

#include "Procedural/ProceduralTileTypes.hpp"
#include "Tile/TileMap.hpp"

#include <glm/glm.hpp>

#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <utility>
#include <vector>

namespace elysium {

struct GeneratedTile {
    int x{0};
    int z{0};

    TileType type{TileType::Grass};
    bool walkable{true};

    float height{0.0f};
    float moisture{0.0f};
    float temperature{0.0f};

    // 8-neighbor signature bitfield (N, NE, E, SE, S, SW, W, NW).
    // Bit = 1 when neighbor has SAME base type.
    std::uint8_t neighborSignature{0};

    float rotationJitterDeg{0.0f};
    float scaleJitter{1.0f};

    glm::vec4 tint{1.0f};
};

struct GeneratedArea {
    int width{0};
    int height{0};
    int originX{0};
    int originZ{0};
    std::vector<GeneratedTile> tiles;
};

struct BiomeNoiseSettings {
    // Macro world shape frequencies.
    float heightFrequency{0.030f};
    float moistureFrequency{0.026f};
    float temperatureFrequency{0.018f};

    // Detail layers (smaller features blended over macro fields).
    float detailFrequency{0.11f};

    int octaves{4};
    float lacunarity{2.0f};
    float gain{0.5f};
};

struct ProceduralAreaParams {
    std::uint32_t seed{1337};
    int width{64};
    int height{64};
    int originX{0};
    int originZ{0};

    float waterLevel{0.30f};
    float beachLevel{0.37f};

    // Blend factor used by downstream mesh generation for transition edges.
    float transitionIntensity{1.0f};

    // Global multiplier for per-type rotation/scale jitter.
    float edgeJitterStrength{1.0f};

    BiomeNoiseSettings noise;

    // Optional per-biome score offset (positive favors this type).
    std::unordered_map<TileType, float, TileTypeHash> biomeWeights;
};

class ProceduralTileGenerator {
public:
    explicit ProceduralTileGenerator(ProceduralTileConfig config = ProceduralTileConfig::defaults())
        : m_config(std::move(config)) {}

    const ProceduralTileConfig& config() const { return m_config; }
    ProceduralTileConfig& config() { return m_config; }

    GeneratedArea generateArea(const ProceduralAreaParams& params) const;
    void applyAreaToTileMap(const GeneratedArea& area, TileMap& tileMap) const;

private:
    float layeredNoise(float x, float z, std::uint32_t seed, const BiomeNoiseSettings& settings) const;
    TileType classifyBiome(float height, float moisture, float temperature, const ProceduralAreaParams& params) const;
    void fillNeighborSignatures(GeneratedArea& area) const;

private:
    ProceduralTileConfig m_config;
};

} // namespace elysium
