// ObjIO.h
// 声明 OBJ 模型文件的读取和保存接口。

#pragma once

#include <filesystem>

#include "mesh/Mesh.h"

namespace mesh {

// 从 OBJ 文件读取三维网格。
Mesh loadObj(const std::filesystem::path& path);

// 将三维网格保存为 OBJ 文件。
void saveObj(const Mesh& mesh, const std::filesystem::path& path);

}  // namespace mesh
