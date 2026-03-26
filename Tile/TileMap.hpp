#pragma once

#include "Walkmesh/Walkmesh.hpp"

#include <glm/glm.hpp>

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace elysium {

struct TileCoord {
    int x{0};
    int z{0};

    bool operator==(const TileCoord& rhs) const {
        return x == rhs.x && z == rhs.z;
    }
};

struct TileCoordHash {
    std::size_t operator()(const TileCoord& coord) const {
        const std::uint64_t ux = static_cast<std::uint64_t>(static_cast<std::uint32_t>(coord.x));
        const std::uint64_t uz = static_cast<std::uint64_t>(static_cast<std::uint32_t>(coord.z));
        return static_cast<std::size_t>((ux << 32U) ^ uz);
    }
};

struct TileData {
    glm::vec4 tint    {0.60f, 0.68f, 0.74f, 1.0f};
    bool      walkable{true};  // false = impassable tile (wall, water, etc.)
};

struct TileInstanceGPU {
    glm::mat4 model{1.0f};
    glm::vec4 tint{1.0f};
};

class TileMap {
public:
    void setTileSize(float tileSize);
    float tileSize() const { return m_tileSize; }

    bool placeTile(int cellX, int cellZ, const TileData& tile);
    bool eraseTile(int cellX, int cellZ);
    bool hasTile(int cellX, int cellZ) const;
    void clear();

    std::size_t tileCount() const { return m_tiles.size(); }
    const std::unordered_map<TileCoord, TileData, TileCoordHash>& tiles() const { return m_tiles; }

    bool worldToCell(const glm::vec3& worldPos, int& outCellX, int& outCellZ) const;
    glm::vec3 cellToWorldCenter(int cellX, int cellZ) const;

    const std::vector<TileInstanceGPU>& instanceData() const;

    // Build a CPU-side flat walkmesh from the current tile set.
    // Each walkable/blocked tile contributes two triangles at Y = 0.
    Walkmesh buildWalkmesh() const;

private:
    void rebuildInstanceCache() const;

private:
    float m_tileSize{1.0f};
    std::unordered_map<TileCoord, TileData, TileCoordHash> m_tiles;

    mutable bool m_cacheDirty{true};
    mutable std::vector<TileInstanceGPU> m_instanceCache;
};

} // namespace elysium
