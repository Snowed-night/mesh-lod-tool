// Mesh.h
// 定义三维网格的基础数据结构和包围盒计算函数。

#pragma once

#include <array>
#include <cstddef>
#include <limits>
#include <vector>

namespace mesh {

// Vec3 表示三维坐标。
struct Vec3 {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

// Face 表示一个三角面。
struct Face {
    // vertex_indices 存储 positions 中的顶点下标。
    std::array<std::size_t, 3> vertex_indices{};
};

// Mesh 表示一个完整三维网格模型。
struct Mesh {
    // positions 存储顶点坐标。
    std::vector<Vec3> positions;
    // faces 存储三角面索引。
    std::vector<Face> faces;
};

// Bounds 表示模型包围盒。
struct Bounds {
    // min/max 分别表示包围盒的最小角点和最大角点。
    Vec3 min;
    Vec3 max;
    // valid 表示包围盒是否由有效顶点计算得到。
    bool valid = false;
};

// 计算模型包围盒。
inline Bounds computeBounds(const Mesh& mesh) {
    Bounds bounds;
    if (mesh.positions.empty()) {
        return bounds;
    }

    const double inf = std::numeric_limits<double>::infinity();
    bounds.min = {inf, inf, inf};
    bounds.max = {-inf, -inf, -inf};

    for (const Vec3& p : mesh.positions) {
        bounds.min.x = p.x < bounds.min.x ? p.x : bounds.min.x;
        bounds.min.y = p.y < bounds.min.y ? p.y : bounds.min.y;
        bounds.min.z = p.z < bounds.min.z ? p.z : bounds.min.z;

        bounds.max.x = p.x > bounds.max.x ? p.x : bounds.max.x;
        bounds.max.y = p.y > bounds.max.y ? p.y : bounds.max.y;
        bounds.max.z = p.z > bounds.max.z ? p.z : bounds.max.z;
    }

    bounds.valid = true;
    return bounds;
}

}  // namespace mesh
