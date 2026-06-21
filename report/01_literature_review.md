# 文献调研报告

题目：误差驱动的三维网格 LOD 简化与可视化系统

## 1. 研究背景

三维模型在游戏、数字孪生、机器人仿真、在线展示和虚拟现实中被大量使用。复杂模型通常包含大量顶点和三角面，如果直接渲染最高精度模型，会带来加载慢、显存占用高、渲染帧率下降等问题。

LOD 是 Level of Detail，意思是“细节层次”。核心思想是根据观察距离、屏幕占比或误差阈值，在高精度模型和低精度模型之间切换。近处模型占屏幕面积大，需要更多三角面；远处模型细节不容易被观察者分辨，可以使用更低精度模型降低渲染和加载成本。

本项目关注的问题是：如何从一个高精度 OBJ/3MF 模型生成多级 LOD，并在 Web 可视化界面中根据视觉误差自动切换，同时展示误差、压缩率、耗时和局部风险热力图。

## 2. 相关技术路线

### 2.1 QEM 网格简化

QEM 是 Quadric Error Metrics，意思是“二次误差度量”。它通过点到相邻三角面平面的距离平方和衡量边折叠误差，是经典三维网格简化方法。

Garland 和 Heckbert 在 SIGGRAPH 1997 提出的 QEM 方法，核心思想是把每个三角面对应的平面误差累加到顶点上。折叠一条边时，将两个端点的误差矩阵相加，并寻找一个让误差最小的新点。该方法能较快生成保形效果较好的低面数模型，是后续许多网格简化方法的重要基础。

本项目中，自实现 QEM 主要用于理解算法原理和作为基线。它包含边界保护和法线翻转检测。边界保护用于减少开口模型轮廓被破坏，法线翻转检测用于避免三角面折叠后朝向反转。

### 2.2 工程化网格处理库

meshoptimizer 是面向实时渲染的开源网格优化库，支持网格简化、顶点缓存优化和压缩等功能。本项目使用其简化结果误差作为工程后端指标。

在真实复杂模型上，完全自实现高鲁棒性的简化器需要处理非流形结构、退化三角形、复杂边界、大规模优先队列更新等问题。课程项目周期有限，因此本项目采用“双后端”策略：自实现 QEM 用于解释原理，meshoptimizer 用于支撑复杂模型和最终展示。

### 2.3 屏幕空间误差

屏幕空间误差是将模型几何误差换算到屏幕像素空间。它常用于实时三维可视化系统，根据用户当前视角决定是否需要更高精度模型。

3D Tiles 规范中使用 Screen-Space Error 作为层级选择依据。Screen-Space Error 可以理解为：如果当前层级被显示到屏幕上，它相对于原始几何在屏幕中造成的像素级误差。允许误差越小，系统越倾向于加载更高精度模型；允许误差越大，系统越倾向于使用低精度模型。

本项目将 meshoptimizer 输出的几何误差与相机距离、视场角、画布高度结合，估计当前 LOD 的屏幕空间误差，并据此实现自适应 LOD 切换。

### 2.4 误差可视化与感知展示

严格几何误差可以使用 Hausdorff 距离等指标评估。Hausdorff 距离表示两个几何集合之间的最大偏差，适合做严谨离线评估，但实时计算成本较高。为了交互展示，本项目没有直接实现完整 Hausdorff 距离，而是实现了视觉风险热力图。

热力图综合局部结构敏感度、当前视角轮廓显著性和 LOD 简化程度。它的作用是解释哪些区域在低精度模型中更容易被观察者注意到，例如尖锐边、薄片结构、人物轮廓和复杂褶皱。

## 3. 本项目定位

本项目不是单纯调用现成库生成低模，而是形成以下完整链路：

```text
OBJ/3MF 输入
    ↓
自实现 QEM 原理基线
    ↓
meshoptimizer 工程化 LOD 生成
    ↓
误差统计与屏幕空间误差估计
    ↓
自适应 LOD 切换
    ↓
Web 可视化、PBR 渲染和误差热力图
```

PBR 是 Physically Based Rendering，意思是“基于物理的渲染”。它通过粗糙度、金属度、环境反射和阴影等因素，让模型显示更接近真实光照效果。

## 4. 本项目可展开的亮点

- 通过自实现 QEM 理解经典算法。
- 通过 meshoptimizer 保证复杂模型上的速度和稳定性。
- 使用屏幕空间误差，而不是简单固定距离切换。
- 使用热力图展示结构敏感区域，使算法结果更可解释。
- 使用 Web 工具将上传、转换、生成、可视化和统计输出串成完整工程流程。

## 5. 参考文献

1. Garland, M., Heckbert, P. S. Surface Simplification Using Quadric Error Metrics. SIGGRAPH 1997. https://www.cs.cmu.edu/~garland/Papers/quadrics.pdf
2. Michael Garland, Quadric-Based Polygonal Surface Simplification. https://mgarland.org/research/quadrics.html
3. meshoptimizer 官方文档。https://meshoptimizer.org/
4. OGC 3D Tiles Specification, Screen-Space Error。https://docs.ogc.org/cs/22-025r4/22-025r4.html
5. QGIS Documentation, Maximum screen space error in 3D Tiles. https://docs.qgis.org/latest/en/docs/user_manual/working_with_3d_tiles/3d_tiles.html
