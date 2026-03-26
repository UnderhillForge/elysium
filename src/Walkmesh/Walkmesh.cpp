#include "Walkmesh/Walkmesh.hpp"

#include <glm/glm.hpp>

#include <cmath>
#include <limits>

namespace elysium {

void Walkmesh::clear() {
    m_triangles.clear();
}

void Walkmesh::addTriangle(const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2,
                            bool walkable) {
    m_triangles.push_back({v0, v1, v2, walkable});
}

// Möller–Trumbore algorithm for ray–triangle intersection.
// We always cast a downward ray so dir = (0, -1, 0).
bool Walkmesh::rayTriangle(
    const glm::vec3& orig, const glm::vec3& dir,
    const glm::vec3& v0,   const glm::vec3& v1, const glm::vec3& v2,
    float& outT)
{
    constexpr float kEps = 1e-7f;

    const glm::vec3 e1 = v1 - v0;
    const glm::vec3 e2 = v2 - v0;
    const glm::vec3 h  = glm::cross(dir, e2);
    const float     a  = glm::dot(e1, h);

    if (std::abs(a) < kEps) return false; // Ray parallel to triangle.

    const float     f  = 1.0f / a;
    const glm::vec3 s  = orig - v0;
    const float     u  = f * glm::dot(s, h);
    if (u < 0.0f || u > 1.0f) return false;

    const glm::vec3 q = glm::cross(s, e1);
    const float     v = f * glm::dot(dir, q);
    if (v < 0.0f || u + v > 1.0f) return false;

    outT = f * glm::dot(e2, q);
    return outT > kEps;
}

std::optional<float> Walkmesh::sampleHeight(float x, float z) const {
    // Cast a downward ray from 1000 m above the query point.
    const glm::vec3 orig{x,    1000.0f, z};
    const glm::vec3 dir {0.0f, -1.0f,  0.0f};

    float bestT = std::numeric_limits<float>::max();
    bool  hit   = false;

    for (const auto& tri : m_triangles) {
        float t = 0.0f;
        if (rayTriangle(orig, dir, tri.v0, tri.v1, tri.v2, t) && t < bestT) {
            bestT = t;
            hit   = true;
        }
    }

    if (!hit) return std::nullopt;
    // dir is -Y so world Y = orig.y - t.
    return orig.y - bestT;
}

bool Walkmesh::isWalkable(float x, float z) const {
    const glm::vec3 orig{x,    1000.0f, z};
    const glm::vec3 dir {0.0f, -1.0f,  0.0f};

    float            bestT   = std::numeric_limits<float>::max();
    const WalkTri*   hitTri  = nullptr;

    for (const auto& tri : m_triangles) {
        float t = 0.0f;
        if (rayTriangle(orig, dir, tri.v0, tri.v1, tri.v2, t) && t < bestT) {
            bestT  = t;
            hitTri = &tri;
        }
    }

    return hitTri != nullptr && hitTri->walkable;
}

// Build line pairs for GL_LINES; one colour per walkability state, called separately.
static std::vector<glm::vec3> buildLines(const std::vector<WalkTri>& tris, bool walkableFilter) {
    std::vector<glm::vec3> lines;
    lines.reserve(tris.size() * 6);
    for (const auto& tri : tris) {
        if (tri.walkable != walkableFilter) continue;
        // Lift lines 2 cm above surface so they don't z-fight with tile geometry.
        const glm::vec3 lift{0.0f, 0.02f, 0.0f};
        lines.push_back(tri.v0 + lift); lines.push_back(tri.v1 + lift);
        lines.push_back(tri.v1 + lift); lines.push_back(tri.v2 + lift);
        lines.push_back(tri.v2 + lift); lines.push_back(tri.v0 + lift);
    }
    return lines;
}

std::vector<glm::vec3> Walkmesh::buildWalkableLines() const {
    return buildLines(m_triangles, true);
}

std::vector<glm::vec3> Walkmesh::buildBlockedLines() const {
    return buildLines(m_triangles, false);
}

} // namespace elysium
