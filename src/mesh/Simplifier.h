// Simplifier.h
// 声明基于 QEM 的网格简化接口。

#pragma once

#include <cstddef>

#include "mesh/Mesh.h"

namespace mesh {

struct SimplifyOptions {
    // target_faces 表示期望保留的三角面数量。
    std::size_t target_faces = 0;
    // preserve_boundary 控制是否保护边界顶点。
    bool preserve_boundary = true;
    // prevent_normal_flip 控制是否拒绝导致三角面翻转的折叠。
    bool prevent_normal_flip = true;
};

enum class SimplifyBackend {
    Qem,
    MeshOptimizer
};

struct SimplifyStats {
    // input_vertices/input_faces 记录简化前规模。
    std::size_t input_vertices = 0;
    std::size_t input_faces = 0;
    // output_vertices/output_faces 记录简化后规模。
    std::size_t output_vertices = 0;
    std::size_t output_faces = 0;
    // collapses 记录实际执行的边折叠次数。
    std::size_t collapses = 0;
    // boundary_rejections 记录因边界保护被拒绝的候选边数量。
    std::size_t boundary_rejections = 0;
    // normal_flip_rejections 记录因法线翻转检测被拒绝的候选边数量。
    std::size_t normal_flip_rejections = 0;
    // normalized_error 表示相对于模型尺度的简化误差。
    double normalized_error = 0.0;
    // absolute_error 表示模型坐标空间中的简化误差。
    double absolute_error = 0.0;
    // reached_target 表示是否达到目标三角面数量。
    bool reached_target = false;
};

struct SimplifyResult {
    // mesh 是简化后的网格。
    Mesh mesh;
    // stats 是本次简化的统计结果。
    SimplifyStats stats;
};

// 将目标比例转换为目标三角面数量。
std::size_t targetFaceCountFromRatio(std::size_t input_faces, double ratio);

// 使用 QEM 边折叠算法简化网格。
SimplifyResult simplifyQem(const Mesh& input, const SimplifyOptions& options);

// 使用 meshoptimizer 简化网格。
SimplifyResult simplifyMeshOptimizer(const Mesh& input, const SimplifyOptions& options);

}  // namespace mesh
