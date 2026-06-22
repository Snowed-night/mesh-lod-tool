// main.js
// 实现 OBJ/3MF 上传、LOD 结果展示和 Three.js 模型预览。

import * as THREE from "three";
import { TrackballControls } from "three/addons/controls/TrackballControls.js";
import { OBJLoader } from "three/addons/loaders/OBJLoader.js";
import { RoomEnvironment } from "three/addons/environments/RoomEnvironment.js";

const canvas = document.querySelector("#viewerCanvas");
const form = document.querySelector("#uploadForm");
const fileInput = document.querySelector("#modelFile");
const fileName = document.querySelector("#fileName");
const processButton = document.querySelector("#processButton");
const statusText = document.querySelector("#statusText");
const modelName = document.querySelector("#modelName");
const lodName = document.querySelector("#lodName");
const convertedObjLink = document.querySelector("#convertedObjLink");
const resultDirText = document.querySelector("#resultDirText");
const lodSlider = document.querySelector("#lodSlider");
const manualModeButton = document.querySelector("#manualModeButton");
const adaptiveModeButton = document.querySelector("#adaptiveModeButton");
const errorThreshold = document.querySelector("#errorThreshold");
const errorThresholdValue = document.querySelector("#errorThresholdValue");
const screenErrorText = document.querySelector("#screenErrorText");
const geometryErrorText = document.querySelector("#geometryErrorText");
const wireButton = document.querySelector("#wireButton");
const resetButton = document.querySelector("#resetButton");
const matteModeButton = document.querySelector("#matteModeButton");
const pbrModeButton = document.querySelector("#pbrModeButton");
const heatmapModeButton = document.querySelector("#heatmapModeButton");
const heatmapMetricText = document.querySelector("#heatmapMetricText");
const heatmapIntensity = document.querySelector("#heatmapIntensity");
const heatmapIntensityValue = document.querySelector("#heatmapIntensityValue");
const lightMode = document.querySelector("#lightMode");
const lightX = document.querySelector("#lightX");
const lightY = document.querySelector("#lightY");
const lightZ = document.querySelector("#lightZ");
const lightIntensity = document.querySelector("#lightIntensity");
const lightColor = document.querySelector("#lightColor");
const statsBody = document.querySelector("#statsBody");
const qualityChart = document.querySelector("#qualityChart");
const qualityContext = qualityChart.getContext("2d");

const renderer = new THREE.WebGLRenderer({ canvas, antialias: true });
renderer.setPixelRatio(Math.min(window.devicePixelRatio, 2));
renderer.setClearColor(0x111418, 1);
renderer.outputColorSpace = THREE.SRGBColorSpace;
renderer.toneMapping = THREE.ACESFilmicToneMapping;
renderer.toneMappingExposure = 0.78;
renderer.shadowMap.enabled = true;
renderer.shadowMap.type = THREE.PCFSoftShadowMap;

const scene = new THREE.Scene();
const pmremGenerator = new THREE.PMREMGenerator(renderer);
scene.environment = pmremGenerator.fromScene(new RoomEnvironment(), 0.04).texture;
const camera = new THREE.PerspectiveCamera(45, 1, 0.01, 1000);
const controls = new TrackballControls(camera, renderer.domElement);
controls.rotateSpeed = 3.0;
controls.zoomSpeed = 1.2;
controls.panSpeed = 0.8;
controls.staticMoving = false;
controls.dynamicDampingFactor = 0.15;
controls.mouseButtons.RIGHT = -1;
controls.mouseButtons.MIDDLE = -1;

const loader = new OBJLoader();
const raycaster = new THREE.Raycaster();
const pointer = new THREE.Vector2();
const matteMaterial = new THREE.MeshLambertMaterial({
  color: 0x9aa3aa,
  emissive: 0x000000,
  side: THREE.DoubleSide
});

const pbrMaterial = new THREE.MeshStandardMaterial({
  color: 0x747c84,
  roughness: 0.82,
  metalness: 0.0,
  envMapIntensity: 0.08,
  side: THREE.DoubleSide
});

const wireMaterial = new THREE.MeshBasicMaterial({
  color: 0xdde7ef,
  wireframe: true
});

const heatmapMaterial = new THREE.MeshBasicMaterial({
  vertexColors: true,
  side: THREE.DoubleSide
});

const heatLowColor = new THREE.Color(0x2f80ed);
const heatMidColor = new THREE.Color(0xf2c94c);
const heatHighColor = new THREE.Color(0xeb5757);

let levels = [];
let currentObject = null;
let currentIndex = 0;
let wireframe = false;
let renderMode = "matte";
let adaptiveMode = false;
let objectCache = new Map();
let loadCache = new Map();
let ambientLight = null;
let keyLight = null;
let fillLight = null;
let pointLight = null;
let pointLightMarker = null;
let groundPlane = null;
let modelCenter = new THREE.Vector3();
let modelRadius = 1;
let lastAdaptiveIndex = -1;
let ctrlLeftPanning = false;
let placingLight = false;
let lastPanPosition = { x: 0, y: 0 };
let heatmapFrame = 0;

// 设置预览场景光照。
function setupLights() {
  ambientLight = new THREE.HemisphereLight(0xf4f8ff, 0x252b31, 0.18);
  scene.add(ambientLight);

  keyLight = new THREE.DirectionalLight(0xffffff, 0.95);
  keyLight.castShadow = true;
  keyLight.shadow.mapSize.set(2048, 2048);
  scene.add(keyLight);
  scene.add(keyLight.target);

  fillLight = new THREE.DirectionalLight(0xb8d4ff, 0.16);
  scene.add(fillLight);
  scene.add(fillLight.target);

  pointLight = new THREE.PointLight(0xffffff, 1.2, 0, 1.4);
  pointLight.castShadow = true;
  pointLight.shadow.mapSize.set(1024, 1024);
  scene.add(pointLight);

  pointLightMarker = new THREE.Mesh(
    new THREE.SphereGeometry(0.12, 24, 16),
    new THREE.MeshBasicMaterial({ color: 0xffffff })
  );
  scene.add(pointLightMarker);

  groundPlane = new THREE.Mesh(
    new THREE.PlaneGeometry(1, 1),
    new THREE.ShadowMaterial({
      color: 0x000000,
      opacity: 0.28
    })
  );
  groundPlane.rotation.x = -Math.PI / 2;
  groundPlane.receiveShadow = true;
  scene.add(groundPlane);

  updateLighting();
}

// 根据显示模式调整环境补光。
function applyRenderLightProfile(mode) {
  if (renderMode === "pbr") {
    pbrMaterial.envMapIntensity = mode === "point" ? 0.035 : 0.08;
    ambientLight.intensity = mode === "point" ? 0.006 : 0.10;
    return;
  }

  pbrMaterial.envMapIntensity = 0.0;
  ambientLight.intensity = mode === "point" ? 0.015 : 0.18;
}

// 根据界面参数更新光源位置和模式。
function updateLighting() {
  if (!keyLight || !pointLight) {
    return;
  }

  const mode = lightMode.value;
  const intensity = Number(lightIntensity.value);
  const color = new THREE.Color(lightColor.value);
  const offsetScale = Math.max(modelRadius, 1);
  const position = modelCenter.clone().add(new THREE.Vector3(
    Number(lightX.value) * offsetScale,
    Number(lightY.value) * offsetScale,
    Number(lightZ.value) * offsetScale
  ));
  const fillPosition = modelCenter.clone().add(new THREE.Vector3(
    -Number(lightX.value) * offsetScale * 0.45,
    Math.max(0.3, Number(lightY.value)) * offsetScale * 0.35,
    -Number(lightZ.value) * offsetScale * 0.45
  ));

  keyLight.visible = mode === "sun";
  fillLight.visible = mode === "sun";
  pointLight.visible = mode === "point";
  pointLightMarker.visible = mode === "point";

  keyLight.color.copy(color);
  keyLight.position.copy(position);
  keyLight.target.position.copy(modelCenter);
  keyLight.intensity = intensity;
  keyLight.shadow.camera.near = modelRadius * 0.05;
  keyLight.shadow.camera.far = modelRadius * 14;
  keyLight.shadow.camera.left = -modelRadius * 4;
  keyLight.shadow.camera.right = modelRadius * 4;
  keyLight.shadow.camera.top = modelRadius * 4;
  keyLight.shadow.camera.bottom = -modelRadius * 4;
  keyLight.shadow.camera.updateProjectionMatrix();

  fillLight.color.set(0xb8d4ff);
  fillLight.position.copy(fillPosition);
  fillLight.target.position.copy(modelCenter);
  fillLight.intensity = Math.max(0.05, intensity * 0.12);

  pointLight.color.copy(color);
  pointLight.position.copy(position);
  pointLight.intensity = intensity;
  pointLight.distance = modelRadius * 30;
  pointLight.decay = 1.0;

  pointLightMarker.position.copy(position);
  pointLightMarker.scale.setScalar(Math.max(modelRadius * 0.08, 0.08));
  pointLightMarker.material.color.copy(color);
  applyRenderLightProfile(mode);
}

// 根据模型包围盒更新接触阴影地面。
function updateGroundPlane(box) {
  if (!groundPlane) {
    return;
  }

  const size = Math.max(modelRadius * 4, 1);
  groundPlane.position.set(modelCenter.x, box.min.y - modelRadius * 0.015, modelCenter.z);
  groundPlane.scale.set(size, size, 1);
}

// 更新页面上的结果目录信息。
function updateResultPath(payload) {
  convertedObjLink.textContent = payload.convertedObjPath || "-";
  convertedObjLink.href = payload.convertedObjUrl || "#";
  resultDirText.textContent = payload.resultDirAbsolutePath || payload.resultDirPath || "-";
  resultDirText.title = payload.resultDirAbsolutePath || payload.resultDirPath || "";
}

// 更新模型中心和尺寸。
function updateModelBounds(object) {
  const box = new THREE.Box3().setFromObject(object);
  const sphere = box.getBoundingSphere(new THREE.Sphere());
  modelCenter.copy(sphere.center);
  modelRadius = Math.max(sphere.radius, 1e-3);
  return { box, sphere };
}

// 重置光源到适合当前模型的位置。
function resetLightForObject() {
  lightX.value = "-1.4";
  lightY.value = "2.2";
  lightZ.value = "2.4";
  if (lightMode.value === "point") {
    lightIntensity.value = "8";
  }
  updateLighting();
}

// 获取当前 LOD 相对原始模型的压缩率。
function reductionRatio(level) {
  const sourceTriangles = levels[0]?.triangles || level.triangles || 1;
  return 1 - level.triangles / sourceTriangles;
}

// 获取当前 LOD 在模型空间中的几何误差。
function geometryError(level) {
  if (!level) {
    return 0;
  }

  if (Number.isFinite(level.absoluteError) && level.absoluteError > 0) {
    return level.absoluteError;
  }
  return Math.max(0, level.normalizedError || 0) * modelRadius * 2;
}

// 计算 1 个屏幕像素对应的模型空间长度。
function worldUnitsPerPixel() {
  const distance = Math.max(0, camera.position.distanceTo(modelCenter) - modelRadius);
  const height = Math.max(1, renderer.domElement.clientHeight);
  return distance * (Math.tan((camera.fov * Math.PI / 180) * 0.5) * 2 / height);
}

// 估计某个 LOD 的屏幕空间误差。
function screenSpaceError(level) {
  const unitsPerPixel = worldUnitsPerPixel();
  if (unitsPerPixel <= 1e-9) {
    return Number.POSITIVE_INFINITY;
  }
  return geometryError(level) / unitsPerPixel;
}

// 格式化质量指标。
function formatMetric(value, digits = 3) {
  if (!Number.isFinite(value)) {
    return "-";
  }
  if (Math.abs(value) >= 1000) {
    return value.toExponential(2);
  }
  if (Math.abs(value) > 0 && Math.abs(value) < 0.001) {
    return value.toExponential(2);
  }
  return value.toFixed(digits);
}

// 混合蓝、黄、红三段颜色。
function heatColor(value) {
  const t = THREE.MathUtils.clamp(value, 0, 1);
  if (t < 0.65) {
    return heatLowColor.clone().lerp(heatMidColor, t / 0.65);
  }
  return heatMidColor.clone().lerp(heatHighColor, (t - 0.65) / 0.35);
}

// 获取顶点的量化位置键。
function positionKey(position, index) {
  const scale = 100000;
  return [
    Math.round(position.getX(index) * scale),
    Math.round(position.getY(index) * scale),
    Math.round(position.getZ(index) * scale)
  ].join(",");
}

// 获取数组中的分位数。
function percentile(values, ratio) {
  if (values.length === 0) {
    return 0;
  }

  const sorted = [...values].sort((a, b) => a - b);
  const index = Math.min(sorted.length - 1, Math.max(0, Math.floor((sorted.length - 1) * ratio)));
  return sorted[index];
}

// 计算顶点局部结构对简化的敏感度。
function computeStructuralFeatureScores(geometry) {
  if (geometry.userData.structuralFeatureScores) {
    return geometry.userData.structuralFeatureScores;
  }

  const position = geometry.getAttribute("position");
  if (!position) {
    return [];
  }

  const groups = new Map();
  const indices = geometry.index?.array;
  const faceCount = indices ? indices.length / 3 : position.count / 3;
  const a = new THREE.Vector3();
  const b = new THREE.Vector3();
  const c = new THREE.Vector3();
  const ab = new THREE.Vector3();
  const ac = new THREE.Vector3();
  const edgeUseCount = new Map();

  function ensureGroup(index) {
    const key = positionKey(position, index);
    if (!groups.has(key)) {
      groups.set(key, {
        indices: [],
        normals: [],
        boundary: 0
      });
    }
    const group = groups.get(key);
    group.indices.push(index);
    return { key, group };
  }

  for (let face = 0; face < faceCount; face += 1) {
    const i0 = indices ? indices[face * 3] : face * 3;
    const i1 = indices ? indices[face * 3 + 1] : face * 3 + 1;
    const i2 = indices ? indices[face * 3 + 2] : face * 3 + 2;
    const g0 = ensureGroup(i0);
    const g1 = ensureGroup(i1);
    const g2 = ensureGroup(i2);

    a.fromBufferAttribute(position, i0);
    b.fromBufferAttribute(position, i1);
    c.fromBufferAttribute(position, i2);
    const faceNormal = ab.subVectors(b, a).cross(ac.subVectors(c, a)).normalize();
    g0.group.normals.push(faceNormal.clone());
    g1.group.normals.push(faceNormal.clone());
    g2.group.normals.push(faceNormal.clone());

    [[g0.key, g1.key], [g1.key, g2.key], [g2.key, g0.key]].forEach((edge) => {
      const edgeKey = edge[0] < edge[1] ? `${edge[0]}|${edge[1]}` : `${edge[1]}|${edge[0]}`;
      edgeUseCount.set(edgeKey, (edgeUseCount.get(edgeKey) || 0) + 1);
    });
  }

  for (const [edgeKey, count] of edgeUseCount.entries()) {
    if (count !== 1) {
      continue;
    }
    const [from, to] = edgeKey.split("|");
    if (groups.has(from)) {
      groups.get(from).boundary = 1;
    }
    if (groups.has(to)) {
      groups.get(to).boundary = 1;
    }
  }

  const rawByKey = new Map();
  const rawValues = [];
  for (const [key, group] of groups.entries()) {
    const average = new THREE.Vector3();
    group.normals.forEach((faceNormal) => average.add(faceNormal));
    average.normalize();

    const deviation = group.normals.length > 1
      ? group.normals.reduce((sum, faceNormal) => sum + (1 - Math.max(-1, Math.min(1, faceNormal.dot(average)))), 0) / group.normals.length
      : 0;
    const raw = deviation * 1.8 + group.boundary * 0.14;
    rawByKey.set(key, raw);
    rawValues.push(raw);
  }

  const low = percentile(rawValues, 0.18);
  const high = Math.max(low + 1e-5, percentile(rawValues, 0.98));
  const scores = new Float32Array(position.count);
  for (const [key, group] of groups.entries()) {
    const normalized = THREE.MathUtils.clamp((rawByKey.get(key) - low) / (high - low), 0, 1);
    const score = Math.pow(normalized, 1.7);
    group.indices.forEach((index) => {
      scores[index] = score;
    });
  }

  geometry.userData.structuralFeatureScores = scores;
  return scores;
}

// 计算当前视角下的轮廓显著性。
function computeSilhouetteScores(geometry) {
  const position = geometry.getAttribute("position");
  const normal = geometry.getAttribute("normal");
  if (!position || !normal) {
    return [];
  }

  const viewDirection = camera.getWorldDirection(new THREE.Vector3()).normalize();
  const currentNormal = new THREE.Vector3();
  const scores = new Float32Array(position.count);
  for (let index = 0; index < position.count; index += 1) {
    currentNormal.fromBufferAttribute(normal, index).normalize();
    const silhouette = 1 - Math.abs(currentNormal.dot(viewDirection));
    scores[index] = silhouette * silhouette;
  }
  return scores;
}

// 为当前 LOD 生成误差热力图顶点颜色。
function applyHeatmapColors(geometry, level) {
  const position = geometry.getAttribute("position");
  if (!position) {
    return;
  }

  const featureScores = computeStructuralFeatureScores(geometry);
  const silhouetteScores = computeSilhouetteScores(geometry);
  const lodSeverity = THREE.MathUtils.clamp(reductionRatio(level), 0, 1);
  const intensity = Number(heatmapIntensity.value || 0.65);
  const colors = new Float32Array(position.count * 3);

  for (let index = 0; index < position.count; index += 1) {
    const feature = featureScores[index] || 0;
    const silhouette = silhouetteScores[index] || 0;
    const baseScore = feature * (0.68 + lodSeverity * 0.22) + silhouette * 0.08;
    const score = THREE.MathUtils.clamp(Math.max(0, baseScore - 0.06) * intensity, 0, 1);
    const color = heatColor(score);
    colors[index * 3] = color.r;
    colors[index * 3 + 1] = color.g;
    colors[index * 3 + 2] = color.b;
  }

  geometry.setAttribute("color", new THREE.BufferAttribute(colors, 3));
}

// 根据当前 LOD 选择网格材质。
function materialForCurrentLevel() {
  if (wireframe) {
    return wireMaterial;
  }
  if (renderMode === "heatmap") {
    return heatmapMaterial;
  }
  if (renderMode === "pbr") {
    return pbrMaterial;
  }
  return matteMaterial;
}

// 更新渲染模式按钮。
function updateRenderModeButtons() {
  matteModeButton.classList.toggle("active", renderMode === "matte");
  pbrModeButton.classList.toggle("active", renderMode === "pbr");
  heatmapModeButton.classList.toggle("active", renderMode === "heatmap");
  heatmapMetricText.textContent = renderMode === "heatmap" ? "蓝低 / 黄中 / 红高" : "未启用";
  updateLighting();
}

// 更新自适应 LOD 指标文字。
function updateQualityReadout() {
  const level = levels[currentIndex];
  if (!level) {
    screenErrorText.textContent = "-";
    geometryErrorText.textContent = "-";
    return;
  }

  screenErrorText.textContent = `${formatMetric(screenSpaceError(level), 2)} px`;
  geometryErrorText.textContent = formatMetric(geometryError(level), 4);
}

// 选择满足屏幕误差阈值的最低复杂度 LOD。
function chooseAdaptiveLevel() {
  if (levels.length === 0) {
    return -1;
  }

  const threshold = Number(errorThreshold.value);
  for (let index = levels.length - 1; index >= 0; index -= 1) {
    if (screenSpaceError(levels[index]) <= threshold) {
      return index;
    }
  }
  return 0;
}

// 根据当前相机位置更新自适应 LOD。
function updateAdaptiveLod() {
  if (!adaptiveMode || levels.length === 0) {
    return;
  }

  const nextIndex = chooseAdaptiveLevel();
  if (nextIndex >= 0 && nextIndex !== currentIndex && nextIndex !== lastAdaptiveIndex) {
    lastAdaptiveIndex = nextIndex;
    lodSlider.value = String(nextIndex);
    loadLevel(nextIndex);
  }
}

// 切换手动 LOD 和自适应 LOD 模式。
function setLodMode(enabled) {
  adaptiveMode = enabled;
  manualModeButton.classList.toggle("active", !adaptiveMode);
  adaptiveModeButton.classList.toggle("active", adaptiveMode);
  lodSlider.disabled = adaptiveMode || levels.length === 0;
  lastAdaptiveIndex = -1;
  if (adaptiveMode) {
    updateAdaptiveLod();
  }
}

// 绘制误差-复杂度质量图。
function drawQualityChart() {
  const rect = qualityChart.getBoundingClientRect();
  const width = Math.max(1, Math.floor(rect.width));
  const height = Math.max(1, Math.floor(rect.height));
  const dpr = Math.min(window.devicePixelRatio || 1, 2);
  qualityChart.width = Math.floor(width * dpr);
  qualityChart.height = Math.floor(height * dpr);
  qualityContext.setTransform(dpr, 0, 0, dpr, 0, 0);
  qualityContext.clearRect(0, 0, width, height);

  const padding = { left: 42, right: 14, top: 12, bottom: 28 };
  const plotWidth = Math.max(1, width - padding.left - padding.right);
  const plotHeight = Math.max(1, height - padding.top - padding.bottom);

  qualityContext.strokeStyle = "#2b3540";
  qualityContext.lineWidth = 1;
  qualityContext.beginPath();
  qualityContext.moveTo(padding.left, padding.top);
  qualityContext.lineTo(padding.left, padding.top + plotHeight);
  qualityContext.lineTo(padding.left + plotWidth, padding.top + plotHeight);
  qualityContext.stroke();

  qualityContext.fillStyle = "#83909d";
  qualityContext.font = "12px Segoe UI, Microsoft YaHei, Arial";
  qualityContext.fillText("误差", 8, padding.top + 8);
  qualityContext.fillText("压缩率", padding.left + plotWidth - 38, height - 8);

  if (levels.length === 0) {
    return;
  }

  const maxError = Math.max(...levels.map((level) => geometryError(level)), 1e-9);
  const points = levels.map((level) => {
    const x = padding.left + reductionRatio(level) * plotWidth;
    const y = padding.top + plotHeight - (geometryError(level) / maxError) * plotHeight;
    return { x, y };
  });

  qualityContext.strokeStyle = "#76b7f2";
  qualityContext.lineWidth = 2;
  qualityContext.beginPath();
  points.forEach((point, index) => {
    if (index === 0) {
      qualityContext.moveTo(point.x, point.y);
    } else {
      qualityContext.lineTo(point.x, point.y);
    }
  });
  qualityContext.stroke();

  points.forEach((point, index) => {
    qualityContext.fillStyle = index === currentIndex ? "#f5c451" : "#8fc7ff";
    qualityContext.beginPath();
    qualityContext.arc(point.x, point.y, index === currentIndex ? 4 : 2.5, 0, Math.PI * 2);
    qualityContext.fill();
  });
}

// 更新光源模式默认值。
function applyLightModeDefaults() {
  if (lightMode.value === "point") {
    lightIntensity.value = String(Math.max(Number(lightIntensity.value), 8));
  } else {
    lightIntensity.value = String(Math.min(Number(lightIntensity.value), 2.4));
  }
  updateLighting();
}

// 设置当前模型材质。
function assignMaterial(node) {
  node.geometry.computeVertexNormals();
  node.castShadow = true;
  node.receiveShadow = true;
  if (renderMode === "heatmap" && !wireframe) {
    applyHeatmapColors(node.geometry, levels[currentIndex]);
  }
  node.material = materialForCurrentLevel();
}

// 清理材质和几何。
function disposeObject(object) {
  object.traverse((node) => {
    if (node.isMesh) {
      node.geometry.dispose();
    }
  });
}

// 根据模型半径设置相机裁剪距离。
function updateCameraClip(radius) {
  camera.near = radius / 100;
  camera.far = radius * 100;
  camera.updateProjectionMatrix();
}

// 设置相机默认位置。
function setCameraForSphere(sphere) {
  camera.position.set(
    sphere.center.x + sphere.radius * 1.8,
    sphere.center.y + sphere.radius * 1.1,
    sphere.center.z + sphere.radius * 2.4
  );
}

// 调整画布尺寸。
function resizeRenderer() {
  const rect = canvas.parentElement.getBoundingClientRect();
  const width = Math.max(1, Math.floor(rect.width));
  const height = Math.max(1, Math.floor(rect.height));
  renderer.setSize(width, height, false);
  camera.aspect = width / height;
  camera.updateProjectionMatrix();
  controls.handleResize();
  drawQualityChart();
}

// 居中显示当前模型。
function fitObject(object) {
  const { box, sphere } = updateModelBounds(object);
  const radius = Math.max(sphere.radius, 1e-3);

  controls.target.copy(sphere.center);
  setCameraForSphere(sphere);
  updateCameraClip(radius);
  updateGroundPlane(box);
  controls.update();
  resetLightForObject();
}

// 沿相机平面平移视角。
function panCamera(deltaX, deltaY) {
  const distance = camera.position.distanceTo(controls.target);
  const height = Math.max(1, renderer.domElement.clientHeight);
  const scale = distance * 2 * Math.tan((camera.fov * Math.PI / 180) * 0.5) / height;
  const right = new THREE.Vector3();
  const up = new THREE.Vector3();
  const forward = new THREE.Vector3();
  camera.matrix.extractBasis(right, up, forward);
  const movement = new THREE.Vector3()
    .addScaledVector(right, -deltaX * scale)
    .addScaledVector(up, deltaY * scale);

  camera.position.add(movement);
  controls.target.add(movement);
  controls.update();
}

// 将屏幕坐标转换为标准化鼠标坐标。
function updatePointerFromEvent(event) {
  const rect = canvas.getBoundingClientRect();
  pointer.x = ((event.clientX - rect.left) / rect.width) * 2 - 1;
  pointer.y = -((event.clientY - rect.top) / rect.height) * 2 + 1;
}

// 将点光源放到指定世界坐标。
function setPointLightWorldPosition(position) {
  const offset = position.clone().sub(modelCenter);
  const scale = Math.max(modelRadius, 1);
  lightX.value = String(THREE.MathUtils.clamp(offset.x / scale, Number(lightX.min), Number(lightX.max)).toFixed(2));
  lightY.value = String(THREE.MathUtils.clamp(offset.y / scale, Number(lightY.min), Number(lightY.max)).toFixed(2));
  lightZ.value = String(THREE.MathUtils.clamp(offset.z / scale, Number(lightZ.min), Number(lightZ.max)).toFixed(2));
  lightMode.value = "point";
  lightIntensity.value = String(Math.max(Number(lightIntensity.value), 12));
  updateLighting();
}

// 在鼠标位置放置点光源。
function placePointLight(event) {
  if (!currentObject) {
    return false;
  }

  updatePointerFromEvent(event);
  raycaster.setFromCamera(pointer, camera);
  const hits = raycaster.intersectObject(currentObject, true);
  if (hits.length > 0) {
    const normal = hits[0].face?.normal?.clone() || new THREE.Vector3();
    normal.transformDirection(hits[0].object.matrixWorld);
    setPointLightWorldPosition(hits[0].point.clone().addScaledVector(normal, modelRadius * 0.18));
    return true;
  }

  const viewPlane = new THREE.Plane().setFromNormalAndCoplanarPoint(
    camera.getWorldDirection(new THREE.Vector3()).normalize(),
    modelCenter
  );
  const fallback = new THREE.Vector3();
  if (raycaster.ray.intersectPlane(viewPlane, fallback)) {
    setPointLightWorldPosition(fallback);
    return true;
  }
  return false;
}

// 绑定 Ctrl + 左键平移，避免右键触发浏览器手势。
function setupCtrlLeftPan() {
  canvas.addEventListener("contextmenu", (event) => {
    event.preventDefault();
  });

  canvas.addEventListener("pointerdown", (event) => {
    if (event.button === 0 && placingLight) {
      event.preventDefault();
      event.stopImmediatePropagation();
      placePointLight(event);
      return;
    }

    if (event.button !== 0 || !event.ctrlKey) {
      return;
    }

    event.preventDefault();
    event.stopImmediatePropagation();
    ctrlLeftPanning = true;
    controls.enabled = false;
    lastPanPosition = { x: event.clientX, y: event.clientY };
    canvas.setPointerCapture(event.pointerId);
  }, true);

  window.addEventListener("pointermove", (event) => {
    if (!ctrlLeftPanning) {
      return;
    }

    event.preventDefault();
    const deltaX = event.clientX - lastPanPosition.x;
    const deltaY = event.clientY - lastPanPosition.y;
    lastPanPosition = { x: event.clientX, y: event.clientY };
    panCamera(deltaX, deltaY);
  }, true);

  window.addEventListener("pointerup", (event) => {
    if (!ctrlLeftPanning) {
      return;
    }

    event.preventDefault();
    ctrlLeftPanning = false;
    controls.enabled = true;
    if (canvas.hasPointerCapture(event.pointerId)) {
      canvas.releasePointerCapture(event.pointerId);
    }
  }, true);
}

// 绑定 L + 左键放置点光源。
function setupPointLightPlacement() {
  window.addEventListener("keydown", (event) => {
    if (event.code === "KeyL") {
      placingLight = true;
      canvas.style.cursor = "crosshair";
    }
  });

  window.addEventListener("keyup", (event) => {
    if (event.code === "KeyL") {
      placingLight = false;
      canvas.style.cursor = "";
    }
  });

  window.addEventListener("blur", () => {
    placingLight = false;
    canvas.style.cursor = "";
  });
}

// 释放旧任务缓存的模型资源。
function clearObjectCache() {
  for (const object of objectCache.values()) {
    disposeObject(object);
  }
  objectCache = new Map();
  loadCache = new Map();
  currentObject = null;
}

// 应用当前材质模式。
function applyMaterial(object) {
  object.traverse((node) => {
    if (node.isMesh) {
      assignMaterial(node);
    }
  });
}

// 刷新所有已缓存模型的显示材质。
function refreshCachedMaterials() {
  for (const [index, object] of objectCache.entries()) {
    const previousIndex = currentIndex;
    currentIndex = index;
    applyMaterial(object);
    currentIndex = previousIndex;
  }
}

// 低频刷新当前模型的视角相关热力图。
function updateVisibleHeatmap() {
  if (renderMode !== "heatmap" || wireframe || !currentObject) {
    return;
  }

  heatmapFrame = (heatmapFrame + 1) % 12;
  if (heatmapFrame !== 0) {
    return;
  }

  currentObject.traverse((node) => {
    if (node.isMesh) {
      applyHeatmapColors(node.geometry, levels[currentIndex]);
      node.geometry.attributes.color.needsUpdate = true;
    }
  });
}

// 获取指定 LOD 的模型对象。
async function getLevelObject(index) {
  if (objectCache.has(index)) {
    return objectCache.get(index);
  }

  if (!loadCache.has(index)) {
    loadCache.set(index, loader.loadAsync(levels[index].url).then((object) => {
      applyMaterial(object);
      objectCache.set(index, object);
      return object;
    }));
  }

  return loadCache.get(index);
}

// 选择首次预览层级。
function initialPreviewIndex() {
  const heavyIndex = levels.findIndex((level) => level.triangles <= 120000);
  return heavyIndex >= 0 ? heavyIndex : Math.max(0, levels.length - 1);
}

// 加载并显示指定 LOD。
async function loadLevel(index, options = {}) {
  if (!levels[index]) {
    return;
  }

  currentIndex = index;
  statusText.textContent = objectCache.has(index) ? "切换中" : "加载模型";
  lodName.textContent = `LOD ${levels[index].level}`;

  const object = await getLevelObject(index);
  applyMaterial(object);

  if (currentObject) {
    scene.remove(currentObject);
  }
  currentObject = object;
  scene.add(currentObject);
  if (options.fit) {
    fitObject(currentObject);
  }
  updateQualityReadout();
  updateStatsTable();
  drawQualityChart();
  statusText.textContent = "预览中";
}

// 更新统计表。
function updateStatsTable() {
  statsBody.innerHTML = "";
  levels.forEach((level, index) => {
    const row = document.createElement("tr");
    row.className = index === currentIndex ? "active-row" : "";
    const screenError = screenSpaceError(level);
    row.innerHTML = `
      <td>LOD ${level.level}</td>
      <td>${level.ratio.toFixed(2)}</td>
      <td>${level.vertices.toLocaleString()}</td>
      <td>${level.triangles.toLocaleString()}</td>
      <td>${(reductionRatio(level) * 100).toFixed(1)}%</td>
      <td>${formatMetric(geometryError(level), 4)}</td>
      <td>${formatMetric(screenError, 2)} px</td>
      <td>${level.simplifyMs.toLocaleString()} ms</td>
    `;
    row.addEventListener("click", () => {
      setLodMode(false);
      lodSlider.value = String(index);
      loadLevel(index);
    });
    statsBody.appendChild(row);
  });
}

// 上传模型并生成 LOD。
async function processModel(event) {
  event.preventDefault();

  if (!fileInput.files.length) {
    statusText.textContent = "请选择模型";
    return;
  }

  const formData = new FormData();
  formData.append("model", fileInput.files[0]);

  processButton.disabled = true;
  statusText.textContent = "转换并生成中";
  modelName.textContent = fileInput.files[0].name;

  try {
    const response = await fetch("/api/lod", {
      method: "POST",
      body: formData
    });
    const payload = await response.json();
    if (!response.ok) {
      throw new Error(payload.error || "LOD generation failed");
    }

    levels = payload.levels;
    lastAdaptiveIndex = -1;
    updateResultPath(payload);
    lodSlider.min = "0";
    lodSlider.max = String(Math.max(0, levels.length - 1));
    lodSlider.disabled = adaptiveMode || levels.length === 0;
    if (currentObject) {
      scene.remove(currentObject);
    }
    clearObjectCache();
    const previewIndex = initialPreviewIndex();
    lodSlider.value = String(previewIndex);
    await loadLevel(previewIndex, { fit: true });
    updateQualityReadout();
    drawQualityChart();
  } catch (error) {
    statusText.textContent = "处理失败";
    console.error(error);
    alert(error.message);
  } finally {
    processButton.disabled = false;
  }
}

setupLights();
setupCtrlLeftPan();
setupPointLightPlacement();
resizeRenderer();
window.addEventListener("resize", resizeRenderer);

fileInput.addEventListener("change", () => {
  fileName.textContent = fileInput.files[0]?.name || "选择 OBJ 或 3MF 模型";
});

form.addEventListener("submit", processModel);

lodSlider.addEventListener("input", () => {
  setLodMode(false);
  loadLevel(Number(lodSlider.value));
});

wireButton.addEventListener("click", () => {
  wireframe = !wireframe;
  wireButton.textContent = wireframe ? "实体" : "线框";
  refreshCachedMaterials();
});

function setRenderMode(nextMode) {
  renderMode = nextMode;
  updateRenderModeButtons();
  refreshCachedMaterials();
}

matteModeButton.addEventListener("click", () => {
  setRenderMode("matte");
});

pbrModeButton.addEventListener("click", () => {
  setRenderMode("pbr");
});

heatmapModeButton.addEventListener("click", () => {
  setRenderMode("heatmap");
});

resetButton.addEventListener("click", () => {
  if (currentObject) {
    fitObject(currentObject);
  }
});

lightMode.addEventListener("input", applyLightModeDefaults);

[lightX, lightY, lightZ, lightIntensity, lightColor].forEach((element) => {
  element.addEventListener("input", updateLighting);
});

manualModeButton.addEventListener("click", () => {
  setLodMode(false);
});

adaptiveModeButton.addEventListener("click", () => {
  setLodMode(true);
});

errorThreshold.addEventListener("input", () => {
  errorThresholdValue.textContent = `${Number(errorThreshold.value).toFixed(1)} px`;
  lastAdaptiveIndex = -1;
  updateAdaptiveLod();
  updateQualityReadout();
  updateStatsTable();
  if (renderMode === "heatmap") {
    refreshCachedMaterials();
  }
  drawQualityChart();
});

heatmapIntensity.addEventListener("input", () => {
  heatmapIntensityValue.textContent = Number(heatmapIntensity.value).toFixed(2);
  if (renderMode === "heatmap") {
    refreshCachedMaterials();
  }
});

updateRenderModeButtons();

// 渲染循环。
function animate() {
  controls.update();
  updateAdaptiveLod();
  updateQualityReadout();
  updateVisibleHeatmap();
  renderer.render(scene, camera);
  requestAnimationFrame(animate);
}

animate();
