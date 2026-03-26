#include "Tile/TileMap.hpp"

#include <glm/gtc/matrix_transform.hpp>

#include <cmath>

namespace elysium {

void TileMap::setTileSize(float tileSize) {
    const float clamped = tileSize < 0.1f ? 0.1f : tileSize;
    if (std::abs(clamped - m_tileSize) < 0.0001f) {
        return;
    }
    m_tileSize = clamped;
    m_cacheDirty = true;
}

bool TileMap::placeTile(int cellX, int cellZ, const TileData& tile) {
    const TileCoord coord{cellX, cellZ};
    auto it = m_tiles.find(coord);
    if (it != m_tiles.end()) {
        it->second = tile;
        m_cacheDirty = true;
        return false;
    }

    m_tiles.emplace(coord, tile);
    m_cacheDirty = true;
    return true;
}

bool TileMap::eraseTile(int cellX, int cellZ) {
    const TileCoord coord{cellX, cellZ};
    const auto erased = m_tiles.erase(coord);
    if (erased > 0U) {
        m_cacheDirty = true;
        return true;
    }
    return false;
}

bool TileMap::hasTile(int cellX, int cellZ) const {
    return m_tiles.contains(TileCoord{cellX, cellZ});
}

void TileMap::clear() {
    if (m_tiles.empty()) {
        return;
    }
    m_tiles.clear();
    m_cacheDirty = true;
}

bool TileMap::worldToCell(const glm::vec3& worldPos, int& outCellX, int& outCellZ) const {
    if (m_tileSize <= 0.0f) {
        return false;
    }

    outCellX = static_cast<int>(std::lround(worldPos.x / m_tileSize));
    outCellZ = static_cast<int>(std::lround(worldPos.z / m_tileSize));
    return true;
}

glm::vec3 TileMap::cellToWorldCenter(int cellX, int cellZ) const {
    return glm::vec3(
        static_cast<float>(cellX) * m_tileSize,
        0.0f,
        static_cast<float>(cellZ) * m_tileSize
    );
}

const std::vector<TileInstanceGPU>& TileMap::instanceData() const {
    if (m_cacheDirty) {
        rebuildInstanceCache();
    }
    return m_instanceCache;
}

void TileMap::rebuildInstanceCache() const {
    m_instanceCache.clear();
    m_instanceCache.reserve(m_tiles.size());

    // Inspired by xoreos/neveredit culling-friendly world data organization:
    // tile cell coords are canonical storage; render instance transforms are generated on demand.
    for (const auto& [coord, tile] : m_tiles) {
        TileInstanceGPU instance{};
        const glm::vec3 center = cellToWorldCenter(coord.x, coord.z);

        glm::mat4 model{1.0f};
        model = glm::translate(model, center + glm::vec3(0.0f, 0.1f, 0.0f));
        model = glm::scale(model, glm::vec3(m_tileSize, 0.2f, m_tileSize));

        instance.model = model;
        instance.tint = tile.tint;
        m_instanceCache.push_back(instance);
    }

    m_cacheDirty = false;
}

Walkmesh TileMap::buildWalkmesh() const {
    Walkmesh wm;
    const float half = m_tileSize * 0.5f;

    for (const auto& [coord, tile] : m_tiles) {
        const glm::vec3 center = cellToWorldCenter(coord.x, coord.z);

        // Split each tile quad along NW–SE diagonal into two CCW triangles at Y = 0.
        // Consistent winding ensures the upward-facing normal for ray-cast hits.
        const glm::vec3 nw{center.x - half, 0.0f, center.z - half};
        const glm::vec3 ne{center.x + half, 0.0f, center.z - half};
        const glm::vec3 se{center.x + half, 0.0f, center.z + half};
        const glm::vec3 sw{center.x - half, 0.0f, center.z + half};

        wm.addTriangle(nw, se, ne, tile.walkable); // tri 1
        wm.addTriangle(nw, sw, se, tile.walkable); // tri 2
    }

    return wm;
}

} // namespace elysium
