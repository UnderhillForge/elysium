#include "Procedural/ProceduralTileGenerator.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <utility>

namespace elysium {

namespace {

constexpr std::uint8_t kNeighborBitN  = 1u << 0u;
constexpr std::uint8_t kNeighborBitNE = 1u << 1u;
constexpr std::uint8_t kNeighborBitE  = 1u << 2u;
constexpr std::uint8_t kNeighborBitSE = 1u << 3u;
constexpr std::uint8_t kNeighborBitS  = 1u << 4u;
constexpr std::uint8_t kNeighborBitSW = 1u << 5u;
constexpr std::uint8_t kNeighborBitW  = 1u << 6u;
constexpr std::uint8_t kNeighborBitNW = 1u << 7u;

std::uint32_t hashCoords(std::int32_t x, std::int32_t z, std::uint32_t seed) {
    // Small integer hash (deterministic and fast) suitable for lattice-noise lookups.
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

float remapSigned(float value01) {
    return value01 * 2.0f - 1.0f;
}

} // namespace

GeneratedArea ProceduralTileGenerator::generateArea(const ProceduralAreaParams& params) const {
    GeneratedArea area{};
    area.width = std::max(1, params.width);
    area.height = std::max(1, params.height);
    area.originX = params.originX;
    area.originZ = params.originZ;
    area.tiles.reserve(static_cast<std::size_t>(area.width * area.height));

    for (int row = 0; row < area.height; ++row) {
        for (int col = 0; col < area.width; ++col) {
            const int cellX = area.originX + col;
            const int cellZ = area.originZ + row;

            const float fx = static_cast<float>(cellX);
            const float fz = static_cast<float>(cellZ);

            const float macroHeight = layeredNoise(fx * params.noise.heightFrequency,
                                                   fz * params.noise.heightFrequency,
                                                   params.seed ^ 0xA341316Cu,
                                                   params.noise);
            const float detailHeight = valueNoise2D(fx * params.noise.detailFrequency,
                                                    fz * params.noise.detailFrequency,
                                                    params.seed ^ 0xC8013EA4u);
            const float height = std::clamp(macroHeight * 0.78f + detailHeight * 0.22f, 0.0f, 1.0f);

            const float moisture = layeredNoise(fx * params.noise.moistureFrequency,
                                                fz * params.noise.moistureFrequency,
                                                params.seed ^ 0xAD90777Du,
                                                params.noise);

            const float temperature = layeredNoise((fx + 137.0f) * params.noise.temperatureFrequency,
                                                   (fz - 251.0f) * params.noise.temperatureFrequency,
                                                   params.seed ^ 0x7E95761Eu,
                                                   params.noise);

            const TileType biome = classifyBiome(height, moisture, temperature, params);
            const TileTypeProperties& props = m_config.properties(biome);

            const float jitterRndA = remapSigned(random01(cellX, cellZ, params.seed ^ 0x1b873593u));
            const float jitterRndB = remapSigned(random01(cellX, cellZ, params.seed ^ 0x85ebca6bu));

            GeneratedTile tile{};
            tile.x = cellX;
            tile.z = cellZ;
            tile.type = biome;
            tile.walkable = props.walkable;
            tile.height = height;
            tile.moisture = moisture;
            tile.temperature = temperature;
            tile.tint = props.baseTint;
            tile.rotationJitterDeg = jitterRndA * props.maxRotationJitterDeg * params.edgeJitterStrength;
            tile.scaleJitter = 1.0f + (jitterRndB * props.maxUniformScaleJitter * params.edgeJitterStrength);

            area.tiles.push_back(tile);
        }
    }

    fillNeighborSignatures(area);
    return area;
}

void ProceduralTileGenerator::applyAreaToTileMap(const GeneratedArea& area, TileMap& tileMap) const {
    for (const auto& tile : area.tiles) {
        TileData data{};
        data.tint = tile.tint;
        data.walkable = tile.walkable;
        tileMap.placeTile(tile.x, tile.z, data);
    }
}

float ProceduralTileGenerator::layeredNoise(float x, float z, std::uint32_t seed, const BiomeNoiseSettings& settings) const {
    float freq = 1.0f;
    float amp = 1.0f;
    float total = 0.0f;
    float totalAmp = 0.0f;

    const int octaves = std::max(1, settings.octaves);
    for (int octave = 0; octave < octaves; ++octave) {
        const std::uint32_t octaveSeed = seed + static_cast<std::uint32_t>(octave) * 0x9e3779b9u;
        total += valueNoise2D(x * freq, z * freq, octaveSeed) * amp;
        totalAmp += amp;
        freq *= settings.lacunarity;
        amp *= settings.gain;
    }

    if (totalAmp <= 0.0f) {
        return 0.0f;
    }
    return std::clamp(total / totalAmp, 0.0f, 1.0f);
}

TileType ProceduralTileGenerator::classifyBiome(float height,
                                                float moisture,
                                                float temperature,
                                                const ProceduralAreaParams& params) const
{
    // First, lock in water + beach shelves from height, then classify land biomes by climate.
    if (height < params.waterLevel) {
        return TileType::Water;
    }
    if (height < params.beachLevel) {
        return TileType::Beach;
    }

    struct Candidate {
        TileType type;
        float score;
    };

    // Scores are intentionally broad to produce smooth macro-biomes.
    std::vector<Candidate> candidates = {
        {TileType::Snow,       (1.0f - temperature) * 1.3f + height * 0.4f},
        {TileType::Swamp,      moisture * 1.3f + (1.0f - height) * 0.25f},
        {TileType::Sand,       (temperature * 0.95f) + (1.0f - moisture) * 0.9f},
        {TileType::Rocky,      height * 0.9f + (1.0f - moisture) * 0.55f},
        {TileType::Cobblestone, height * 0.65f + (0.5f - std::abs(moisture - 0.5f)) * 0.4f},
        {TileType::Grass,      moisture * 0.75f + temperature * 0.55f},
    };

    for (auto& candidate : candidates) {
        const auto it = params.biomeWeights.find(candidate.type);
        if (it != params.biomeWeights.end()) {
            candidate.score += it->second;
        }
    }

    auto best = std::max_element(candidates.begin(), candidates.end(), [](const Candidate& a, const Candidate& b) {
        return a.score < b.score;
    });

    if (best == candidates.end()) {
        return TileType::Grass;
    }
    return best->type;
}

void ProceduralTileGenerator::fillNeighborSignatures(GeneratedArea& area) const {
    if (area.tiles.empty() || area.width <= 0 || area.height <= 0) {
        return;
    }

    auto idxOf = [&](int col, int row) -> std::size_t {
        return static_cast<std::size_t>(row * area.width + col);
    };

    auto sameTypeAt = [&](int col, int row, TileType selfType) -> bool {
        if (col < 0 || col >= area.width || row < 0 || row >= area.height) {
            return false;
        }
        return area.tiles[idxOf(col, row)].type == selfType;
    };

    for (int row = 0; row < area.height; ++row) {
        for (int col = 0; col < area.width; ++col) {
            GeneratedTile& tile = area.tiles[idxOf(col, row)];
            std::uint8_t sig = 0;
            if (sameTypeAt(col, row - 1, tile.type)) sig |= kNeighborBitN;
            if (sameTypeAt(col + 1, row - 1, tile.type)) sig |= kNeighborBitNE;
            if (sameTypeAt(col + 1, row, tile.type)) sig |= kNeighborBitE;
            if (sameTypeAt(col + 1, row + 1, tile.type)) sig |= kNeighborBitSE;
            if (sameTypeAt(col, row + 1, tile.type)) sig |= kNeighborBitS;
            if (sameTypeAt(col - 1, row + 1, tile.type)) sig |= kNeighborBitSW;
            if (sameTypeAt(col - 1, row, tile.type)) sig |= kNeighborBitW;
            if (sameTypeAt(col - 1, row - 1, tile.type)) sig |= kNeighborBitNW;
            tile.neighborSignature = sig;
        }
    }
}

} // namespace elysium
