# Mesh LOD Tool

面向计算机图形学课程项目的三维网格简化与 LOD 可视化工具。

项目仓库：

```text
https://github.com/Snowed-night/mesh-lod-tool
```

LOD 是 Level of Detail，意思是“细节层次”：近处显示高精度模型，远处显示低精度模型，用更少三角面换取更低渲染成本。

## 当前功能

- 读取和保存 OBJ 三角网格模型
- 生成可控组合几何测试模型
- 使用自实现 QEM 后端做基线简化
- 使用 `meshoptimizer` 后端生成更稳定的多级 LOD
- 输出每级 LOD 的顶点数、三角面数和耗时统计
- 使用 OpenGL 窗口查看模型、线框和多级 LOD
- 支持基于屏幕空间误差的自适应 LOD 切换
- Web 工具支持上传 OBJ 和基础 3MF，并自动生成 LOD
- Web 工具支持 PBR 材质、光源调节、接触阴影和误差热力图

QEM 是 Quadric Error Metrics，意思是“二次误差度量”。它用误差矩阵评估边折叠后模型形状偏离原模型的程度。
屏幕空间误差是把几何误差换算成屏幕像素误差，用来判断当前视角下某个 LOD 是否足够精细。

## 构建

推荐使用 CMake：

```powershell
cmake -S . -B build
cmake --build build
```

如果当前环境 CMake 不方便使用，可以用 MinGW 直接编译：

```powershell
New-Item -ItemType Directory -Force build-manual | Out-Null
g++ -std=c++17 -Wall -Wextra -Wpedantic -Isrc -Ithird_party/meshoptimizer/src src/main.cpp src/mesh/ObjIO.cpp src/mesh/PrimitiveMeshes.cpp src/mesh/Simplifier.cpp src/mesh/MeshOptimizerSimplifier.cpp src/viewer/Viewer.cpp third_party/meshoptimizer/src/simplifier.cpp third_party/meshoptimizer/src/allocator.cpp -lopengl32 -lgdi32 -o build-manual/mesh_lod_tool.exe
```

## 常用命令

生成可控组合几何模型：

```powershell
New-Item -ItemType Directory -Force result\web\manual\compound | Out-Null
.\build-manual\mesh_lod_tool.exe create-compound result\web\manual\compound\compound.obj
```

使用 `meshoptimizer` 生成 28 档 LOD 和统计表：

```powershell
.\build-manual\mesh_lod_tool.exe generate-lod result\web\manual\compound\compound.obj result\web\manual\compound\meshopt result\web\tables\compound_meshopt_stats.csv --backend meshopt
```

使用自实现 QEM 生成对照结果：

```powershell
.\build-manual\mesh_lod_tool.exe generate-lod result\web\manual\compound\compound.obj result\web\manual\compound\qem result\web\tables\compound_qem_stats.csv --backend qem
```

查看单个模型信息：

```powershell
.\build-manual\mesh_lod_tool.exe info result\web\manual\compound\meshopt\lod_040.obj
```

打开自动 LOD 查看窗口：

```powershell
.\build-manual\mesh_lod_tool.exe view-lod result\web\manual\compound\meshopt\lod_100.obj result\web\manual\compound\meshopt\lod_080.obj result\web\manual\compound\meshopt\lod_060.obj result\web\manual\compound\meshopt\lod_040.obj result\web\manual\compound\meshopt\lod_020.obj result\web\manual\compound\meshopt\lod_010.obj result\web\manual\compound\meshopt\lod_004.obj result\web\manual\compound\meshopt\lod_001.obj
```

窗口操作：

- 鼠标左键拖拽：旋转模型
- 鼠标滚轮：改变视角远近；自动模式下会触发 LOD 切换
- `1` 到 `9`：手动切换第 1 到第 9 个模型
- `0`：手动切换第 10 个模型
- `[` / `]`：切换上一个 / 下一个 LOD
- `A`：切换自动 LOD 和手动 LOD
- `W`：切换实体模式和线框模式
- `R`：重置相机
- `Esc`：关闭窗口

## 输出目录

本项目的实验输出统一放在 `result/`：

```text
result/
  web/
    models/
      <job>/
        source.obj
        lod_100.obj
        lod_095.obj
        ...
        lod_001.obj
    tables/
      <job>_stats.csv
    jobs.json
```

原始模型统一放在 `素材/下载模型/`，截图和视频统一放在 `素材/截图或者视频/`。每个下载模型单独建文件夹保存，Web 工具生成的 LOD 结果统一放在 `result/web/`。

## 当前验证结果

`compound.obj` 原始模型规模：

- 顶点数：148
- 三角面数：270

`meshoptimizer` 默认生成 28 档 LOD：

| LOD | 比例 | 三角面 |
| --- | ---: | ---: |
| 100 | 1.00 | 270 |
| 095 | 0.95 | 按模型计算 |
| 090 | 0.90 | 按模型计算 |
| ... | ... | ... |
| 010 | 0.10 | 按模型计算 |
| 004 | 0.04 | 按模型计算 |
| 001 | 0.01 | 按模型计算 |

完整统计表由 Web 工具写入 `result/web/tables/<job>_stats.csv`。

## 真实模型测试

真实模型保存在 `素材/下载模型/`，每个模型一个独立文件夹。Web 后端会把上传文件复制或转换成 `result/web/models/<job>/source.obj`，再调用 C++ 工具生成 LOD。

```text
素材/下载模型/girl/
素材/下载模型/horse/
素材/下载模型/kobe/
素材/下载模型/TV Furniture/
素材/下载模型/IndoorPotPlant/
素材/下载模型/compound_test/
```

同一个模型重复上传时，Web 后端会根据文件哈希复用已有结果，不再重复生成一套时间戳目录。

## Web 预览工具

`web/` 目录提供一个本地 Web 工具：

- 前端：上传 OBJ/3MF、切换 LOD、线框/实体预览、调节光源、查看统计表
- 后端：调用 `build-manual/mesh_lod_tool.exe generate-lod` 生成 LOD
- 渲染：浏览器使用 Three.js 加载 OBJ

启动方式：

```powershell
cd web
npm install
npm start
```

浏览器打开：

```text
http://localhost:5177
```

当前 Web 版支持 OBJ 和基础 3MF 上传。3MF 会先在后端转换为 OBJ，再进入 LOD 管线。上传后的源 OBJ 会自动保存到本次任务目录，例如 `result/web/models/<job>/source.obj`。如果上传文件来自 `素材/下载模型/`，后端会把 3MF 转换出的 OBJ 同步保存回原模型目录。再次上传完全相同的文件时，后端会复用 `jobs.json` 中记录的结果。

Web 预览操作：

- 左键拖拽：自由旋转模型
- Ctrl + 左键拖拽：平移视角
- 鼠标滚轮：缩放视角
- 手动 LOD：通过滑块选择指定 LOD
- 自适应 LOD：根据屏幕空间误差阈值自动选择最低复杂度 LOD
- 真实材质：使用 PBR 材质、环境光和接触阴影展示模型
- 误差热力图：用蓝、黄、红显示局部结构风险，热力图强度可调
- 光源：可在自然光和点光源之间切换；点光源支持 X/Y/Z 位置、强度和颜色调节
- `L` + 左键：在模型表面或当前视图平面上放置点光源

已验证模型：

- `horse+3d+model.3mf`：190,272 三角面，多档 LOD 生成成功。
- `girl.3mf`：332,940 三角面，多档 LOD 生成成功。
- `empire+children.3mf`：1,331,674 三角面，多档 LOD 生成成功。
- `TV Furniture.obj`、`indoor plant_02.obj`：用于测试真实物品模型的渲染和热力图效果。

STL、FBX、CAD 文件建议后续作为新的“模型格式预处理模块”加入，先转换为 OBJ 再进入 LOD 管线。

这不是在浏览器里重写简化算法，而是前端负责交互，后端复用 C++ 简化核心。这样更稳定，也更适合作为课程项目中的工程化展示。

## 代码阅读顺序

1. `src/mesh/Mesh.h`：核心网格数据结构
2. `src/mesh/ObjIO.h` 和 `src/mesh/ObjIO.cpp`：OBJ 文件读写
3. `src/mesh/Simplifier.h`：简化后端统一接口和统计字段
4. `src/mesh/Simplifier.cpp`：自实现 QEM 基线
5. `src/mesh/MeshOptimizerSimplifier.cpp`：`meshoptimizer` 后端封装
6. `src/mesh/PrimitiveMeshes.cpp`：可控几何模型生成
7. `src/viewer/Viewer.h` 和 `src/viewer/Viewer.cpp`：OpenGL 可视化和自动 LOD
8. `src/main.cpp`：命令行入口和实验输出流程

## 课程报告拆分建议

最终大作业可以拆成三份平时作业材料和一份答辩 PPT：

```text
report/
  01_literature_review.md   文献调研报告
  02_technical_report.md    技术报告：代码、实验、结果
  03_reflection.md          未来工作展望与个人感想
  ppt_outline.md            答辩 PPT 结构
```

建议最终压缩包命名：

```text
学号+姓名+误差驱动的三维网格LOD简化与可视化系统.zip
```

打包时建议包含 `src/`、`web/`、`third_party/meshoptimizer/`、`README.md`、`report/`、代表性 `result/web/tables/*.csv` 和必要截图。`result/web/models/` 体积较大，可以只保留 1 到 2 个代表模型，或在文档中说明可通过命令复现生成。

## 下一步

下一阶段建议录制 2 组对比素材：手动 LOD 由精到粗变化、自适应 LOD 随视角远近自动切换。报告重点突出“自实现 QEM 原理基线 + meshoptimizer 工程后端 + 屏幕空间误差自适应切换 + 误差热力图/PBR 展示”。

