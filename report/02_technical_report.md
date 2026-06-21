# 技术报告提纲

题目：误差驱动的三维网格 LOD 简化与可视化系统

## 1. 项目目标

实现一个可复现、可展示、可扩展的三维网格 LOD 工具。系统支持 OBJ/3MF 输入，生成多级低精度模型，并通过 Web 界面展示手动 LOD、自适应 LOD、统计表、光照和误差热力图。

## 2. 系统结构

```text
C++ 工具
  OBJ 读写
  QEM 基线简化
  meshoptimizer 简化后端
  LOD 批量生成
  CSV 统计输出

Web 后端
  模型上传
  3MF 转 OBJ
  调用 C++ 工具
  缓存与去重

Web 前端
  Three.js 模型加载
  手动 / 自适应 LOD
  PBR 渲染
  点光源控制
  误差热力图
  统计表和曲线
```

## 3. 核心算法

### 3.1 QEM 基线

说明点到平面的距离平方和、顶点误差矩阵、边折叠代价、边界保护和法线翻转检测。

### 3.2 meshoptimizer 后端

说明为什么真实复杂模型采用成熟库：速度、鲁棒性、复杂拓扑处理更稳定。

### 3.3 屏幕空间误差自适应 LOD

```text
world_units_per_pixel =
distance * (2 * tan(fovy / 2) / screen_height)

screen_error =
geometry_error / world_units_per_pixel
```

选择策略：

```text
从低精度 LOD 开始检查；
选择第一个 screen_error <= threshold 的 LOD；
如果都不满足，则使用最高精度 LOD。
```

### 3.4 误差热力图

热力图不等价于严格 Hausdorff 距离，而是实时结构风险可视化：

```text
heat_score =
局部结构敏感度
+ 当前视角轮廓显著性
+ LOD 简化程度权重
```

## 4. 实验结果

当前已有统计表：

```text
result/web/tables/tv_furniture_752d373f3c76_stats.csv
result/web/tables/indoor_plant_02_eafb7985f897_stats.csv
result/web/tables/horse_3d_model_4b0c341d9387_stats.csv
result/web/tables/girl_118fd17336c9_stats.csv
result/web/tables/empire_children_e259041b8b89_stats.csv
```

建议正文选 3 个代表模型：

- TV Furniture：物品模型，适合展示外观保持。
- IndoorPotPlant：薄片和复杂结构，适合展示热力图调参。
- Horse 或 Girl：复杂生物/人物模型，适合展示高面数模型的 LOD 效果。

## 5. 运行方式

```powershell
cmake -S . -B build
cmake --build build
```

或：

```powershell
g++ -std=c++17 -Wall -Wextra -Wpedantic -Isrc -Ithird_party/meshoptimizer/src src/main.cpp src/mesh/ObjIO.cpp src/mesh/PrimitiveMeshes.cpp src/mesh/Simplifier.cpp src/mesh/MeshOptimizerSimplifier.cpp src/viewer/Viewer.cpp third_party/meshoptimizer/src/simplifier.cpp third_party/meshoptimizer/src/allocator.cpp -lopengl32 -lgdi32 -o build-manual/mesh_lod_tool.exe
```

Web 启动：

```powershell
cd web
npm install
npm start
```

## 6. 结果展示建议

- 截图 1：Web 主界面。
- 截图 2：同一模型不同 LOD 对比。
- 截图 3：自适应 LOD 模式下的屏幕误差和当前 LOD。
- 截图 4：误差热力图。
- 表格：不同模型的顶点数、三角面数、压缩率、误差和耗时。
