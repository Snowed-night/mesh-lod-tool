// Simplifier.cpp
// 实现基于 QEM 的边折叠网格简化。

#include "mesh/Simplifier.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <map>
#include <stdexcept>
#include <vector>

namespace mesh {
namespace {

constexpr double kEpsilon = 1e-12;

struct Quadric {
    // m 存储 QEM 使用的 4x4 矩阵。
    std::array<std::array<double, 4>, 4> m{};
};

struct WorkFace {
    // vertex_indices 存储当前工作网格中的三角面顶点下标。
    std::array<std::size_t, 3> vertex_indices{};
    // active 表示该三角面是否仍参与简化。
    bool active = true;
};

struct EdgeKey {
    // a/b 存储排序后的边端点下标。
    std::size_t a = 0;
    std::size_t b = 0;

    EdgeKey() = default;

    EdgeKey(std::size_t first, std::size_t second) {
        a = std::min(first, second);
        b = std::max(first, second);
    }

    bool operator<(const EdgeKey& other) const {
        return a < other.a || (a == other.a && b < other.b);
    }
};

struct Topology {
    // edge_face_counts 记录每条边被多少个活动三角面使用。
    std::map<EdgeKey, std::size_t> edge_face_counts;
    // boundary_vertices 标记位于边界上的顶点。
    std::vector<bool> boundary_vertices;
};

struct CollapseCandidate {
    // edge 是候选折叠边。
    EdgeKey edge;
    // position 是折叠后的新顶点位置。
    Vec3 position;
    // cost 是该候选边的折叠代价。
    double cost = std::numeric_limits<double>::infinity();
    // valid 表示该候选是否可用。
    bool valid = false;
};

// 计算两个三维坐标之和。
Vec3 add(const Vec3& lhs, const Vec3& rhs) {
    return {lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z};
}

// 计算两个三维坐标之差。
Vec3 subtract(const Vec3& lhs, const Vec3& rhs) {
    return {lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z};
}

// 计算三维坐标的数乘结果。
Vec3 multiply(const Vec3& value, double scale) {
    return {value.x * scale, value.y * scale, value.z * scale};
}

// 计算两个三维向量的点积。
double dot(const Vec3& lhs, const Vec3& rhs) {
    return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z;
}

// 计算两个三维向量的叉积。
Vec3 cross(const Vec3& lhs, const Vec3& rhs) {
    return {
        lhs.y * rhs.z - lhs.z * rhs.y,
        lhs.z * rhs.x - lhs.x * rhs.z,
        lhs.x * rhs.y - lhs.y * rhs.x
    };
}

// 计算三维向量长度。
double length(const Vec3& value) {
    return std::sqrt(dot(value, value));
}

// 计算两个三维坐标的中点。
Vec3 midpoint(const Vec3& lhs, const Vec3& rhs) {
    return multiply(add(lhs, rhs), 0.5);
}

// 计算两个 Qi 矩阵之和。
Quadric addQuadrics(const Quadric& lhs, const Quadric& rhs) {
    Quadric result;
    for (std::size_t row = 0; row < 4; ++row) {
        for (std::size_t col = 0; col < 4; ++col) {
            result.m[row][col] = lhs.m[row][col] + rhs.m[row][col];
        }
    }
    return result;
}

// 将一个平面方程累加到 Qi 矩阵中。
void addPlane(Quadric& quadric, double a, double b, double c, double d) {
    const std::array<double, 4> p = {a, b, c, d};
    for (std::size_t row = 0; row < 4; ++row) {
        for (std::size_t col = 0; col < 4; ++col) {
            quadric.m[row][col] += p[row] * p[col];
        }
    }
}

// 计算候选点在 Q 矩阵下的折叠代价。
double quadricError(const Quadric& quadric, const Vec3& position) {
    const std::array<double, 4> v = {position.x, position.y, position.z, 1.0};
    double error = 0.0;
    for (std::size_t row = 0; row < 4; ++row) {
        for (std::size_t col = 0; col < 4; ++col) {
            error += v[row] * quadric.m[row][col] * v[col];
        }
    }
    return error;
}

// 求解 3x3 线性方程组。
bool solve3x3(std::array<std::array<double, 3>, 3> a, std::array<double, 3> b, Vec3& result) {
    for (std::size_t col = 0; col < 3; ++col) {
        std::size_t pivot = col;
        for (std::size_t row = col + 1; row < 3; ++row) {
            if (std::abs(a[row][col]) > std::abs(a[pivot][col])) {
                pivot = row;
            }
        }

        if (std::abs(a[pivot][col]) < kEpsilon) {
            return false;
        }

        if (pivot != col) {
            std::swap(a[pivot], a[col]);
            std::swap(b[pivot], b[col]);
        }

        const double pivot_value = a[col][col];
        for (std::size_t j = col; j < 3; ++j) {
            a[col][j] /= pivot_value;
        }
        b[col] /= pivot_value;

        for (std::size_t row = 0; row < 3; ++row) {
            if (row == col) {
                continue;
            }

            const double factor = a[row][col];
            for (std::size_t j = col; j < 3; ++j) {
                a[row][j] -= factor * a[col][j];
            }
            b[row] -= factor * b[col];
        }
    }

    result = {b[0], b[1], b[2]};
    return true;
}

// 计算一条边折叠后的候选新位置。
Vec3 bestPosition(const Quadric& quadric, const Vec3& first, const Vec3& second) {
    std::array<std::array<double, 3>, 3> a{};
    std::array<double, 3> b{};
    for (std::size_t row = 0; row < 3; ++row) {
        for (std::size_t col = 0; col < 3; ++col) {
            a[row][col] = quadric.m[row][col];
        }
        b[row] = -quadric.m[row][3];
    }

    Vec3 solved;
    if (solve3x3(a, b, solved)) {
        return solved;
    }

    const Vec3 mid = midpoint(first, second);
    Vec3 best = first;
    double best_error = quadricError(quadric, best);

    const double second_error = quadricError(quadric, second);
    if (second_error < best_error) {
        best = second;
        best_error = second_error;
    }

    const double mid_error = quadricError(quadric, mid);
    if (mid_error < best_error) {
        best = mid;
    }

    return best;
}

// 计算三角面的未归一化法线。
Vec3 faceNormal(const std::vector<Vec3>& positions, const std::array<std::size_t, 3>& indices) {
    const Vec3 e1 = subtract(positions[indices[1]], positions[indices[0]]);
    const Vec3 e2 = subtract(positions[indices[2]], positions[indices[0]]);
    return cross(e1, e2);
}

// 判断三角面是否出现重复顶点。
bool hasDuplicateIndex(const std::array<std::size_t, 3>& indices) {
    return indices[0] == indices[1] || indices[1] == indices[2] || indices[0] == indices[2];
}

// 向拓扑表加入一条边。
void addEdge(Topology& topology, std::size_t first, std::size_t second) {
    ++topology.edge_face_counts[EdgeKey(first, second)];
}

// 构建当前工作网格的边和边界顶点信息。
Topology buildTopology(
    const std::vector<WorkFace>& faces,
    const std::vector<bool>& active_vertices
) {
    Topology topology;
    topology.boundary_vertices.assign(active_vertices.size(), false);

    for (const WorkFace& face : faces) {
        if (!face.active) {
            continue;
        }

        addEdge(topology, face.vertex_indices[0], face.vertex_indices[1]);
        addEdge(topology, face.vertex_indices[1], face.vertex_indices[2]);
        addEdge(topology, face.vertex_indices[2], face.vertex_indices[0]);
    }

    for (const auto& entry : topology.edge_face_counts) {
        if (entry.second == 1) {
            topology.boundary_vertices[entry.first.a] = true;
            topology.boundary_vertices[entry.first.b] = true;
        }
    }

    return topology;
}

// 计算每个顶点的 Qi 矩阵。
std::vector<Quadric> buildQuadrics(
    const std::vector<Vec3>& positions,
    const std::vector<WorkFace>& faces
) {
    std::vector<Quadric> quadrics(positions.size());

    for (const WorkFace& face : faces) {
        if (!face.active) {
            continue;
        }

        const Vec3 p0 = positions[face.vertex_indices[0]];
        const Vec3 p1 = positions[face.vertex_indices[1]];
        const Vec3 p2 = positions[face.vertex_indices[2]];
        Vec3 normal = cross(subtract(p1, p0), subtract(p2, p0));

        const double normal_length = length(normal);
        if (normal_length < kEpsilon) {
            continue;
        }

        normal = multiply(normal, 1.0 / normal_length);
        const double d = -dot(normal, p0);

        Quadric plane;
        addPlane(plane, normal.x, normal.y, normal.z, d);

        for (const std::size_t index : face.vertex_indices) {
            quadrics[index] = addQuadrics(quadrics[index], plane);
        }
    }

    return quadrics;
}

// 判断边折叠是否会导致相邻三角面法线翻转。
bool wouldFlipNormals(
    const std::vector<Vec3>& positions,
    const std::vector<WorkFace>& faces,
    std::size_t first,
    std::size_t second,
    const Vec3& candidate_position
) {
    std::vector<Vec3> updated_positions = positions;
    updated_positions[first] = candidate_position;
    updated_positions[second] = candidate_position;

    for (const WorkFace& face : faces) {
        if (!face.active) {
            continue;
        }

        const bool uses_first = face.vertex_indices[0] == first || face.vertex_indices[1] == first || face.vertex_indices[2] == first;
        const bool uses_second = face.vertex_indices[0] == second || face.vertex_indices[1] == second || face.vertex_indices[2] == second;

        if (!uses_first && !uses_second) {
            continue;
        }

        if (uses_first && uses_second) {
            continue;
        }

        const Vec3 old_normal = faceNormal(positions, face.vertex_indices);
        const Vec3 new_normal = faceNormal(updated_positions, face.vertex_indices);
        const double old_length = length(old_normal);
        const double new_length = length(new_normal);

        if (old_length < kEpsilon) {
            continue;
        }

        if (new_length < kEpsilon) {
            return true;
        }

        if (dot(old_normal, new_normal) <= 0.0) {
            return true;
        }
    }

    return false;
}

// 在当前拓扑中选择代价最小的合法折叠边。
CollapseCandidate findBestCandidate(
    const std::vector<Vec3>& positions,
    const std::vector<WorkFace>& faces,
    const std::vector<bool>& active_vertices,
    const std::vector<Quadric>& quadrics,
    const Topology& topology,
    const SimplifyOptions& options,
    SimplifyStats& stats
) {
    CollapseCandidate best;

    for (const auto& entry : topology.edge_face_counts) {
        const EdgeKey edge = entry.first;
        if (!active_vertices[edge.a] || !active_vertices[edge.b]) {
            continue;
        }

        if (options.preserve_boundary && (topology.boundary_vertices[edge.a] || topology.boundary_vertices[edge.b])) {
            ++stats.boundary_rejections;
            continue;
        }

        const Quadric combined = addQuadrics(quadrics[edge.a], quadrics[edge.b]);
        const Vec3 position = bestPosition(combined, positions[edge.a], positions[edge.b]);

        if (options.prevent_normal_flip && wouldFlipNormals(positions, faces, edge.a, edge.b, position)) {
            ++stats.normal_flip_rejections;
            continue;
        }

        const double cost = quadricError(combined, position);
        if (!best.valid || cost < best.cost) {
            best.edge = edge;
            best.position = position;
            best.cost = cost;
            best.valid = true;
        }
    }

    return best;
}

// 执行一次边折叠并更新活动三角面数量。
void collapseEdge(
    std::vector<Vec3>& positions,
    std::vector<WorkFace>& faces,
    std::vector<bool>& active_vertices,
    std::size_t& active_face_count,
    const CollapseCandidate& candidate
) {
    const std::size_t keep = candidate.edge.a;
    const std::size_t remove = candidate.edge.b;

    positions[keep] = candidate.position;
    active_vertices[remove] = false;

    for (WorkFace& face : faces) {
        if (!face.active) {
            continue;
        }

        for (std::size_t& index : face.vertex_indices) {
            if (index == remove) {
                index = keep;
            }
        }

        if (hasDuplicateIndex(face.vertex_indices)) {
            face.active = false;
            --active_face_count;
        }
    }
}

// 压缩工作网格，移除失效顶点和失效三角面。
Mesh compactMesh(const std::vector<Vec3>& positions, const std::vector<WorkFace>& faces) {
    constexpr std::size_t kInvalidIndex = std::numeric_limits<std::size_t>::max();

    Mesh output;
    std::vector<std::size_t> remap(positions.size(), kInvalidIndex);

    for (const WorkFace& face : faces) {
        if (!face.active || hasDuplicateIndex(face.vertex_indices)) {
            continue;
        }

        Face output_face;
        for (std::size_t i = 0; i < 3; ++i) {
            const std::size_t old_index = face.vertex_indices[i];
            if (remap[old_index] == kInvalidIndex) {
                remap[old_index] = output.positions.size();
                output.positions.push_back(positions[old_index]);
            }
            output_face.vertex_indices[i] = remap[old_index];
        }
        output.faces.push_back(output_face);
    }

    return output;
}

}  // namespace

// 将目标比例转换为目标三角面数量。
std::size_t targetFaceCountFromRatio(std::size_t input_faces, double ratio) {
    if (ratio <= 0.0 || ratio > 1.0) {
        throw std::runtime_error("simplify ratio must be in range (0, 1]");
    }

    if (input_faces == 0) {
        return 0;
    }

    const auto target = static_cast<std::size_t>(std::ceil(static_cast<double>(input_faces) * ratio));
    return std::max<std::size_t>(1, target);
}

// 使用 QEM 边折叠算法简化网格。
SimplifyResult simplifyQem(const Mesh& input, const SimplifyOptions& options) {
    SimplifyResult result;
    result.stats.input_vertices = input.positions.size();
    result.stats.input_faces = input.faces.size();

    if (input.faces.empty() || options.target_faces >= input.faces.size()) {
        result.mesh = input;
        result.stats.output_vertices = input.positions.size();
        result.stats.output_faces = input.faces.size();
        result.stats.reached_target = true;
        return result;
    }

    const std::size_t target_faces = std::max<std::size_t>(1, options.target_faces);

    std::vector<Vec3> positions = input.positions;
    std::vector<bool> active_vertices(positions.size(), true);
    std::vector<WorkFace> faces;
    faces.reserve(input.faces.size());

    for (const Face& face : input.faces) {
        faces.push_back(WorkFace{face.vertex_indices, true});
    }

    std::size_t active_face_count = input.faces.size();

    while (active_face_count > target_faces) {
        const Topology topology = buildTopology(faces, active_vertices);
        const std::vector<Quadric> quadrics = buildQuadrics(positions, faces);

        const CollapseCandidate candidate = findBestCandidate(
            positions,
            faces,
            active_vertices,
            quadrics,
            topology,
            options,
            result.stats
        );

        if (!candidate.valid) {
            break;
        }

        collapseEdge(positions, faces, active_vertices, active_face_count, candidate);
        ++result.stats.collapses;
    }

    result.mesh = compactMesh(positions, faces);
    result.stats.output_vertices = result.mesh.positions.size();
    result.stats.output_faces = result.mesh.faces.size();
    result.stats.reached_target = result.stats.output_faces <= target_faces;
    return result;
}

}  // namespace mesh
