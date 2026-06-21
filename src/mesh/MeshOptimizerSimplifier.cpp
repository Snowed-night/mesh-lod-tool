// MeshOptimizerSimplifier.cpp
// 实现基于 meshoptimizer 的网格简化后端。

#include "mesh/Simplifier.h"

#include "meshoptimizer.h"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <vector>

namespace mesh {
namespace {

// 将 Mesh 顶点转换为 meshoptimizer 使用的 float3 缓冲。
std::vector<float> buildPositionBuffer(const Mesh& input) {
    std::vector<float> positions;
    positions.reserve(input.positions.size() * 3);
    for (const Vec3& p : input.positions) {
        positions.push_back(static_cast<float>(p.x));
        positions.push_back(static_cast<float>(p.y));
        positions.push_back(static_cast<float>(p.z));
    }
    return positions;
}

// 将 Mesh 三角面转换为索引缓冲。
std::vector<unsigned int> buildIndexBuffer(const Mesh& input) {
    if (input.positions.size() > static_cast<std::size_t>(std::numeric_limits<unsigned int>::max())) {
        throw std::runtime_error("meshoptimizer backend requires fewer than 2^32 vertices");
    }

    std::vector<unsigned int> indices;
    indices.reserve(input.faces.size() * 3);
    for (const Face& face : input.faces) {
        indices.push_back(static_cast<unsigned int>(face.vertex_indices[0]));
        indices.push_back(static_cast<unsigned int>(face.vertex_indices[1]));
        indices.push_back(static_cast<unsigned int>(face.vertex_indices[2]));
    }
    return indices;
}

// 根据简化后的索引缓冲生成紧凑 Mesh。
Mesh compactIndexedMesh(const Mesh& input, const std::vector<unsigned int>& indices) {
    constexpr std::size_t kInvalidIndex = std::numeric_limits<std::size_t>::max();

    Mesh output;
    std::vector<std::size_t> remap(input.positions.size(), kInvalidIndex);

    for (std::size_t i = 0; i + 2 < indices.size(); i += 3) {
        Face face;
        for (std::size_t j = 0; j < 3; ++j) {
            const std::size_t old_index = indices[i + j];
            if (old_index >= input.positions.size()) {
                throw std::runtime_error("meshoptimizer returned an out-of-range vertex index");
            }

            if (remap[old_index] == kInvalidIndex) {
                remap[old_index] = output.positions.size();
                output.positions.push_back(input.positions[old_index]);
            }
            face.vertex_indices[j] = remap[old_index];
        }
        output.faces.push_back(face);
    }

    return output;
}

}  // namespace

// 使用 meshoptimizer 简化网格。
SimplifyResult simplifyMeshOptimizer(const Mesh& input, const SimplifyOptions& options) {
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

    const std::vector<float> positions = buildPositionBuffer(input);
    const std::vector<unsigned int> indices = buildIndexBuffer(input);
    std::vector<unsigned int> simplified(indices.size());

    const std::size_t target_index_count = std::max<std::size_t>(3, options.target_faces * 3);
    const float target_error = 1.0f;
    float result_error = 0.0f;
    const float error_scale = meshopt_simplifyScale(
        positions.data(),
        input.positions.size(),
        sizeof(float) * 3
    );

    unsigned int meshopt_options = meshopt_SimplifyRegularizeLight;
    if (options.preserve_boundary) {
        meshopt_options |= meshopt_SimplifyLockBorder;
    } else {
        meshopt_options |= meshopt_SimplifyPrune;
    }

    const std::size_t simplified_count = meshopt_simplify(
        simplified.data(),
        indices.data(),
        indices.size(),
        positions.data(),
        input.positions.size(),
        sizeof(float) * 3,
        target_index_count,
        target_error,
        meshopt_options,
        &result_error
    );

    simplified.resize(simplified_count - simplified_count % 3);
    result.mesh = compactIndexedMesh(input, simplified);
    result.stats.output_vertices = result.mesh.positions.size();
    result.stats.output_faces = result.mesh.faces.size();
    result.stats.collapses = result.stats.input_faces > result.stats.output_faces
        ? result.stats.input_faces - result.stats.output_faces
        : 0;
    result.stats.normalized_error = result_error;
    result.stats.absolute_error = static_cast<double>(result_error) * static_cast<double>(error_scale);
    result.stats.reached_target = result.stats.output_faces <= options.target_faces;
    return result;
}

}  // namespace mesh
