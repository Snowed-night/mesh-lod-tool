// Viewer.h
// 声明 OpenGL 网格查看器接口。

#pragma once

#include <string>
#include <vector>

#include "mesh/Mesh.h"

namespace viewer {

struct ViewerModel {
    // name 表示窗口切换时显示的模型名称。
    std::string name;
    // mesh 是需要显示的三维网格。
    mesh::Mesh mesh;
};

// 打开 OpenGL 窗口并显示一个或多个网格模型。
int runViewer(const std::vector<ViewerModel>& models, bool auto_lod = false);

}  // namespace viewer
