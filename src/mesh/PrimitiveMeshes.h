// PrimitiveMeshes.h
// 声明用于实验验证的程序生成网格。

#pragma once

#include "mesh/Mesh.h"

namespace mesh {

// 生成由箱体、斜坡和柱体组成的组合几何模型。
Mesh createCompoundMesh();

}  // namespace mesh
