// PrimitiveMeshes.cpp
// 实现用于实验验证的程序生成网格。

#include "mesh/PrimitiveMeshes.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <vector>

namespace mesh {
namespace {

constexpr double kPi = 3.14159265358979323846;

// 将顶点加入网格并返回下标。
std::size_t addVertex(Mesh& mesh, const Vec3& position) {
    mesh.positions.push_back(position);
    return mesh.positions.size() - 1;
}

// 向网格加入一个三角面。
void addTriangle(Mesh& mesh, std::size_t a, std::size_t b, std::size_t c) {
    mesh.faces.push_back(Face{{a, b, c}});
}

// 向网格加入一个四边形面。
void addQuad(Mesh& mesh, std::size_t a, std::size_t b, std::size_t c, std::size_t d) {
    addTriangle(mesh, a, b, c);
    addTriangle(mesh, a, c, d);
}

// 向网格加入一个长方体。
void addBox(Mesh& mesh, const Vec3& center, const Vec3& size) {
    const double hx = size.x * 0.5;
    const double hy = size.y * 0.5;
    const double hz = size.z * 0.5;

    const std::array<std::size_t, 8> v = {
        addVertex(mesh, {center.x - hx, center.y - hy, center.z - hz}),
        addVertex(mesh, {center.x + hx, center.y - hy, center.z - hz}),
        addVertex(mesh, {center.x + hx, center.y + hy, center.z - hz}),
        addVertex(mesh, {center.x - hx, center.y + hy, center.z - hz}),
        addVertex(mesh, {center.x - hx, center.y - hy, center.z + hz}),
        addVertex(mesh, {center.x + hx, center.y - hy, center.z + hz}),
        addVertex(mesh, {center.x + hx, center.y + hy, center.z + hz}),
        addVertex(mesh, {center.x - hx, center.y + hy, center.z + hz}),
    };

    addQuad(mesh, v[0], v[1], v[2], v[3]);
    addQuad(mesh, v[4], v[7], v[6], v[5]);
    addQuad(mesh, v[0], v[4], v[5], v[1]);
    addQuad(mesh, v[1], v[5], v[6], v[2]);
    addQuad(mesh, v[2], v[6], v[7], v[3]);
    addQuad(mesh, v[4], v[0], v[3], v[7]);
}

// 向网格加入一个斜坡。
void addRamp(Mesh& mesh, const Vec3& center, const Vec3& size) {
    const double hx = size.x * 0.5;
    const double hy = size.y * 0.5;
    const double hz = size.z * 0.5;

    const std::array<std::size_t, 8> v = {
        addVertex(mesh, {center.x - hx, center.y - hy, center.z - hz}),
        addVertex(mesh, {center.x + hx, center.y - hy, center.z - hz}),
        addVertex(mesh, {center.x + hx, center.y + hy, center.z - hz}),
        addVertex(mesh, {center.x - hx, center.y + hy, center.z - hz}),
        addVertex(mesh, {center.x - hx, center.y - hy, center.z - hz}),
        addVertex(mesh, {center.x + hx, center.y - hy, center.z - hz}),
        addVertex(mesh, {center.x + hx, center.y + hy, center.z + hz}),
        addVertex(mesh, {center.x - hx, center.y + hy, center.z + hz}),
    };

    addQuad(mesh, v[0], v[1], v[2], v[3]);
    addQuad(mesh, v[3], v[2], v[6], v[7]);
    addQuad(mesh, v[0], v[3], v[7], v[4]);
    addQuad(mesh, v[1], v[5], v[6], v[2]);
    addQuad(mesh, v[4], v[7], v[6], v[5]);
}

// 向网格加入一个多边形柱体。
void addCylinder(Mesh& mesh, const Vec3& center, double radius, double height, std::size_t segments) {
    const double half_height = height * 0.5;
    const std::size_t bottom_center = addVertex(mesh, {center.x, center.y, center.z - half_height});
    const std::size_t top_center = addVertex(mesh, {center.x, center.y, center.z + half_height});
    std::vector<std::size_t> bottom_ring;
    std::vector<std::size_t> top_ring;
    bottom_ring.reserve(segments);
    top_ring.reserve(segments);

    for (std::size_t i = 0; i < segments; ++i) {
        const double angle = 2.0 * kPi * static_cast<double>(i) / static_cast<double>(segments);
        bottom_ring.push_back(addVertex(mesh, {center.x + std::cos(angle) * radius, center.y + std::sin(angle) * radius, center.z - half_height}));
        top_ring.push_back(addVertex(mesh, {center.x + std::cos(angle) * radius, center.y + std::sin(angle) * radius, center.z + half_height}));
    }

    for (std::size_t i = 0; i < segments; ++i) {
        const std::size_t next = (i + 1) % segments;
        const std::size_t b0 = bottom_ring[i];
        const std::size_t b1 = bottom_ring[next];
        const std::size_t t0 = top_ring[i];
        const std::size_t t1 = top_ring[next];

        addTriangle(mesh, bottom_center, b1, b0);
        addTriangle(mesh, top_center, t0, t1);
        addQuad(mesh, b0, b1, t1, t0);
    }
}

}  // namespace

Mesh createCompoundMesh() {
    Mesh mesh;
    addBox(mesh, {0.0, 0.0, 0.2}, {4.0, 3.0, 0.4});
    addBox(mesh, {-0.9, -0.4, 0.9}, {1.0, 0.8, 1.0});
    addBox(mesh, {1.1, 0.7, 0.75}, {1.2, 0.7, 0.7});
    addRamp(mesh, {0.3, -1.0, 0.65}, {1.6, 0.9, 0.9});
    addCylinder(mesh, {-1.3, 0.9, 0.85}, 0.35, 1.3, 32);
    addCylinder(mesh, {1.45, -0.75, 0.75}, 0.28, 1.1, 24);
    return mesh;
}

}  // namespace mesh
