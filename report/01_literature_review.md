# 文献调研报告提纲

题目：误差驱动的三维网格 LOD 简化与可视化系统

## 1. 研究背景

三维模型在游戏、数字孪生、机器人仿真、在线展示和虚拟现实中被大量使用。复杂模型通常包含大量顶点和三角面，如果直接渲染最高精度模型，会带来加载慢、显存占用高、渲染帧率下降等问题。

LOD 是 Level of Detail，意思是多细节层次。核心思想是根据观察距离、屏幕占比或误差阈值，在高精度模型和低精度模型之间切换。

## 2. 相关技术路线

### 2.1 QEM 网格简化

QEM 是 Quadric Error Metrics，意思是二次误差度量。它通过点到相邻三角面平面的距离平方和衡量边折叠误差，是经典三维网格简化方法。

可引用文献：

- Garland, M., Heckbert, P. S. Surface Simplification Using Quadric Error Metrics. SIGGRAPH 1997.

### 2.2 工程化网格处理库

meshoptimizer 是面向实时渲染的开源网格优化库，支持网格简化、顶点缓存优化和压缩等功能。本项目使用其简化结果误差作为工程后端指标。

### 2.3 屏幕空间误差

屏幕空间误差是将模型几何误差换算到屏幕像素空间。它常用于实时三维可视化系统，根据用户当前视角决定是否需要更高精度模型。

可对标方向：

- 3D Tiles / Cesium 中的 Screen-Space Error。
- 实时渲染中基于距离、投影尺寸和误差阈值的 LOD 策略。

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

## 4. 可写入正文的亮点

- 通过自实现 QEM 理解经典算法。
- 通过 meshoptimizer 保证复杂模型上的速度和稳定性。
- 使用屏幕空间误差，而不是简单固定距离切换。
- 使用热力图展示结构敏感区域，使算法结果更可解释。

## 5. 参考文献占位

1. Garland, M., Heckbert, P. S. Surface Simplification Using Quadric Error Metrics. SIGGRAPH 1997.
2. meshoptimizer 官方文档。
3. OGC 3D Tiles Specification 中关于 Screen-Space Error 的说明。
