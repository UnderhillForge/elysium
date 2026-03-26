#pragma once

#include <glm/glm.hpp>

#include <optional>
#include <vector>

namespace elysium {

// A single triangle in the walkmesh.
// walkable = false marks impassable geometry (walls, objects, water edges).
struct WalkTri {
    glm::vec3 v0{};
    glm::vec3 v1{};
    glm::vec3 v2{};
    bool walkable{true};
};

// CPU-side triangle-soup walkmesh.
//
// Design notes (concepts inspired by NWN ASWM walkmesh format):
//  - Each tile contributes two flat triangles (a quad split along the diagonal) at Y = 0.
//  - glTF mesh geometry can be extracted and appended for height-accurate terrain later.
//  - Height queries use a downward Möller–Trumbore ray cast over all triangles (O(n)).
//    A future pass will partition triangles into a spatial grid/quadtree for O(log n).
//  - Blocked tiles mark their triangles walkable = false so the character controller
//    refuses to step onto them, useful for walls, water, and restricted zones.
class Walkmesh {
public:
    void clear();

    void addTriangle(const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2,
                     bool walkable = true);

    std::size_t triangleCount() const { return m_triangles.size(); }
    const std::vector<WalkTri>& triangles() const { return m_triangles; }

    // Given world (x, z), return the Y of the highest hit triangle, or nullopt if none.
    std::optional<float> sampleHeight(float x, float z) const;

    // Returns true if the first hit triangle at (x, z) is walkable.
    bool isWalkable(float x, float z) const;

    // Build a flat vertex list for GL_LINES debug rendering.
    // Each triangle produces 6 vertices (3 edges × 2 endpoints).
    // Pass walkable = true for walkable tris (green), false for blocked (red).
    std::vector<glm::vec3> buildWalkableLines() const;
    std::vector<glm::vec3> buildBlockedLines() const;

private:
    // Möller–Trumbore downward ray–triangle intersection.
    // orig: ray origin high above scene. dir: (0, -1, 0).
    // Returns true and sets outT to the hit distance along the ray.
    static bool rayTriangle(
        const glm::vec3& orig, const glm::vec3& dir,
        const glm::vec3& v0,   const glm::vec3& v1, const glm::vec3& v2,
        float& outT);

    std::vector<WalkTri> m_triangles;
};

} // namespace elysium
