// server.js
// 提供 OBJ 上传、LOD 生成和静态资源访问服务。

import childProcess from "node:child_process";
import crypto from "node:crypto";
import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import express from "express";
import multer from "multer";
import JSZip from "jszip";
import { XMLParser } from "fast-xml-parser";

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);
const projectRoot = path.resolve(__dirname, "..");
const toolPath = path.join(projectRoot, "build-manual", "mesh_lod_tool.exe");
const workRoot = path.join(projectRoot, "result", "web");
const uploadRoot = path.join(workRoot, "uploads");
const modelRoot = path.join(workRoot, "models");
const tableRoot = path.join(workRoot, "tables");
const tempRoot = path.join(workRoot, "temp");
const jobIndexPath = path.join(workRoot, "jobs.json");
const materialRoot = path.join(projectRoot, "素材", "下载模型");

const app = express();
const upload = multer({
  storage: multer.memoryStorage(),
  limits: {
    fileSize: 80 * 1024 * 1024
  }
});

// 确保 Web 工具需要的输出目录存在。
function ensureDirs() {
  fs.mkdirSync(uploadRoot, { recursive: true });
  fs.mkdirSync(modelRoot, { recursive: true });
  fs.mkdirSync(tableRoot, { recursive: true });
  fs.mkdirSync(tempRoot, { recursive: true });
}

// 生成安全的文件名主体。
function safeBaseName(filename) {
  const parsed = path.parse(filename);
  const base = parsed.name.toLowerCase().replace(/[^a-z0-9_-]+/g, "_").replace(/^_+|_+$/g, "");
  return base || "model";
}

// 计算文件内容哈希。
function fileHash(buffer) {
  const hash = crypto.createHash("sha256");
  hash.update(buffer);
  return hash.digest("hex");
}

// 计算磁盘文件哈希。
function diskFileHash(filePath) {
  return fileHash(fs.readFileSync(filePath));
}

// 读取上传任务索引。
function readJobIndex() {
  if (!fs.existsSync(jobIndexPath)) {
    return {};
  }

  return JSON.parse(fs.readFileSync(jobIndexPath, "utf8"));
}

// 保存上传任务索引。
function writeJobIndex(index) {
  fs.writeFileSync(jobIndexPath, JSON.stringify(index, null, 2), "utf8");
}

// 执行命令并返回 stdout。
function runCommand(command, args) {
  return new Promise((resolve, reject) => {
    childProcess.execFile(command, args, { cwd: projectRoot, windowsHide: true }, (error, stdout, stderr) => {
      if (error) {
        reject(new Error(stderr || stdout || error.message));
        return;
      }
      resolve(stdout);
    });
  });
}

// 转换为 C++ 工具可稳定处理的相对路径。
function toolRelative(filePath) {
  return path.relative(projectRoot, filePath).split(path.sep).join("/");
}

// 读取 LOD 统计 CSV。
function readStats(csvPath) {
  const text = fs.readFileSync(csvPath, "utf8").trim();
  const lines = text.split(/\r?\n/);
  const headers = lines.shift().split(",");
  return lines.map((line) => {
    const cells = line.split(",");
    const row = {};
    headers.forEach((header, index) => {
      row[header] = cells[index] ?? "";
    });
    return row;
  });
}

// 将统计结果转换为前端使用的 LOD 列表。
function levelsFromStats(stats) {
  return stats.map((row) => ({
    level: row.level,
    ratio: Number(row.ratio),
    vertices: Number(row.output_vertices),
    triangles: Number(row.output_faces),
    normalizedError: Number(row.normalized_error || 0),
    absoluteError: Number(row.absolute_error || 0),
    simplifyMs: Number(row.simplify_ms || 0),
    url: modelUrl(row.file)
  }));
}

// 判断缓存任务是否可复用。
function cachedJobReady(job) {
  if (!job) {
    return false;
  }

  if (!fs.existsSync(job.statsPath)
    || !fs.existsSync(job.sourceObjPath)
    || !fs.existsSync(job.outputDir)) {
    return false;
  }

  const header = fs.readFileSync(job.statsPath, "utf8").split(/\r?\n/, 1)[0] || "";
  return header.includes("normalized_error") && header.includes("absolute_error");
}

// 生成接口响应数据。
function jobResponse(job, originalName, cached) {
  const stats = readStats(job.statsPath);
  return {
    job: job.name,
    originalName,
    cached,
    convertedObjUrl: modelUrl(job.sourceObjPath),
    convertedObjPath: toolRelative(job.sourceObjPath),
    resultDirPath: toolRelative(job.outputDir),
    resultDirAbsolutePath: job.outputDir,
    materialObjPath: job.materialObjPath || "",
    stats,
    levels: levelsFromStats(stats)
  };
}

// 删除临时上传文件。
function removeIfExists(filePath) {
  if (fs.existsSync(filePath)) {
    fs.rmSync(filePath, { force: true });
  }
}

// 清空上传临时目录。
function cleanupUploadRoot() {
  if (!fs.existsSync(uploadRoot)) {
    return;
  }

  for (const entry of fs.readdirSync(uploadRoot, { withFileTypes: true })) {
    if (entry.isFile()) {
      removeIfExists(path.join(uploadRoot, entry.name));
    }
  }
}

// 查找素材目录中与上传文件同名且内容相同的文件。
function findMatchingMaterialFile(originalName, hash) {
  if (!fs.existsSync(materialRoot)) {
    return "";
  }

  const stack = [materialRoot];
  while (stack.length > 0) {
    const current = stack.pop();
    for (const entry of fs.readdirSync(current, { withFileTypes: true })) {
      const fullPath = path.join(current, entry.name);
      if (entry.isDirectory()) {
        stack.push(fullPath);
        continue;
      }
      if (entry.isFile() && entry.name === originalName && diskFileHash(fullPath) === hash) {
        return fullPath;
      }
    }
  }
  return "";
}

// 将转换后的 OBJ 归档到原始素材同目录。
function archiveConvertedObjToMaterial(sourceObjPath, materialFilePath) {
  if (!materialFilePath) {
    return "";
  }

  const parsed = path.parse(materialFilePath);
  const targetPath = path.join(parsed.dir, `${parsed.name}.obj`);
  fs.copyFileSync(sourceObjPath, targetPath);
  return targetPath;
}

// 将本地路径转换为浏览器可访问 URL。
function modelUrl(filePath) {
  const absolute = path.isAbsolute(filePath) ? filePath : path.join(projectRoot, filePath);
  const relative = path.relative(workRoot, absolute).split(path.sep).join("/");
  return `/files/${relative}`;
}

// 解析 3MF 的 3x4 变换矩阵。
function parseTransform(text) {
  if (!text) {
    return [1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0];
  }

  const values = String(text).trim().split(/\s+/).map(Number);
  if (values.length !== 12 || values.some((value) => Number.isNaN(value))) {
    return [1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0];
  }
  return values;
}

// 组合两个 3MF 变换矩阵。
function multiplyTransform(parent, child) {
  const result = new Array(12).fill(0);
  for (let row = 0; row < 3; row += 1) {
    for (let col = 0; col < 3; col += 1) {
      result[row * 3 + col] =
        parent[row * 3 + 0] * child[col + 0] +
        parent[row * 3 + 1] * child[col + 3] +
        parent[row * 3 + 2] * child[col + 6];
    }
  }
  result[9] = parent[0] * child[9] + parent[1] * child[10] + parent[2] * child[11] + parent[9];
  result[10] = parent[3] * child[9] + parent[4] * child[10] + parent[5] * child[11] + parent[10];
  result[11] = parent[6] * child[9] + parent[7] * child[10] + parent[8] * child[11] + parent[11];
  return result;
}

// 对顶点应用 3MF 变换矩阵。
function transformPoint(position, transform) {
  return {
    x: position.x * transform[0] + position.y * transform[3] + position.z * transform[6] + transform[9],
    y: position.x * transform[1] + position.y * transform[4] + position.z * transform[7] + transform[10],
    z: position.x * transform[2] + position.y * transform[5] + position.z * transform[8] + transform[11]
  };
}

// 从 3MF model XML 中提取对象列表。
function modelObjects(model) {
  return Array.isArray(model.resources?.object)
    ? model.resources.object
    : [model.resources?.object].filter(Boolean);
}

// 从 3MF mesh 对象中提取网格。
function meshFromObject(object, transform) {
  const mesh = object.mesh;
  if (!mesh || !mesh.vertices || !mesh.triangles) {
    return null;
  }

  const vertices = Array.isArray(mesh.vertices.vertex) ? mesh.vertices.vertex : [mesh.vertices.vertex].filter(Boolean);
  const triangles = Array.isArray(mesh.triangles.triangle) ? mesh.triangles.triangle : [mesh.triangles.triangle].filter(Boolean);
  const positions = vertices.map((vertex) => transformPoint({
    x: Number(vertex["@_x"]),
    y: Number(vertex["@_y"]),
    z: Number(vertex["@_z"])
  }, transform));
  const faces = triangles.map((triangle) => [
    Number(triangle["@_v1"]),
    Number(triangle["@_v2"]),
    Number(triangle["@_v3"])
  ]);
  return { positions, faces };
}

// 从 3MF 片段中直接提取网格数据。
function extractMeshFrom3mfText(text, transform) {
  const vertexPattern = /<vertex\b[^>]*\bx="([^"]+)"[^>]*\by="([^"]+)"[^>]*\bz="([^"]+)"[^>]*\/>/g;
  const trianglePattern = /<triangle\b[^>]*\bv1="([^"]+)"[^>]*\bv2="([^"]+)"[^>]*\bv3="([^"]+)"[^>]*\/>/g;

  const rawPositions = [];
  const rawFaces = [];
  let match;

  while ((match = vertexPattern.exec(text)) !== null) {
    rawPositions.push(transformPoint({
      x: Number(match[1]),
      y: Number(match[2]),
      z: Number(match[3])
    }, transform));
  }

  while ((match = trianglePattern.exec(text)) !== null) {
    rawFaces.push([Number(match[1]), Number(match[2]), Number(match[3])]);
  }

  if (rawPositions.length === 0 || rawFaces.length === 0) {
    return null;
  }

  return { positions: rawPositions, faces: rawFaces };
}

// 读取 3MF 外部对象文件并提取网格。
async function read3mfObjectMesh(zip, filePath, transform) {
  const normalized = filePath.replace(/^\/+/, "");
  const file = zip.file(normalized);
  if (!file) {
    throw new Error(`3MF referenced file is missing: ${filePath}`);
  }

  const text = await file.async("text");
  const mesh = extractMeshFrom3mfText(text, transform);
  if (!mesh) {
    throw new Error(`3MF referenced mesh is empty: ${filePath}`);
  }
  return mesh;
}

// 收集主 3MF 文档中的网格。
async function collectMeshesFromModel(zip, model, meshes) {
  const buildItems = Array.isArray(model.build?.item) ? model.build.item : [model.build?.item].filter(Boolean);

  if (buildItems.length === 0) {
    for (const object of modelObjects(model)) {
      const directMesh = meshFromObject(object, parseTransform(null));
      if (directMesh) {
        meshes.push(directMesh);
      }
    }
    return;
  }

  const visitedFiles = new Set();
  for (const item of buildItems) {
    const object = modelObjects(model).find((entry) => String(entry["@_id"]) === String(item["@_objectid"]));
    if (!object) {
      continue;
    }

    const buildTransform = parseTransform(item["@_transform"]);
    const directMesh = meshFromObject(object, buildTransform);
    if (directMesh) {
      meshes.push(directMesh);
      continue;
    }

    const components = Array.isArray(object.components?.component)
      ? object.components.component
      : [object.components?.component].filter(Boolean);

    for (const component of components) {
      const componentTransform = multiplyTransform(buildTransform, parseTransform(component["@_transform"]));
      const componentPath = component["@_path"];
      if (!componentPath) {
        continue;
      }

      const normalized = componentPath.replace(/^\/+/, "");
      const visitKey = `${normalized}:${componentTransform.join(",")}`;
      if (visitedFiles.has(visitKey)) {
        continue;
      }
      visitedFiles.add(visitKey);
      meshes.push(await read3mfObjectMesh(zip, componentPath, componentTransform));
    }
  }
}

// 将网格列表写出为 OBJ 文件。
function writeObjFromMeshes(meshEntries, targetPath) {
  const fd = fs.openSync(targetPath, "w");
  try {
    fs.writeSync(fd, "# Converted from 3MF by Mesh LOD Tool\n");

    for (const entry of meshEntries) {
      for (const position of entry.positions) {
        fs.writeSync(fd, `v ${position.x} ${position.y} ${position.z}\n`);
      }
    }

    let offset = 0;
    for (const entry of meshEntries) {
      for (const face of entry.faces) {
        fs.writeSync(fd, `f ${face[0] + offset + 1} ${face[1] + offset + 1} ${face[2] + offset + 1}\n`);
      }
      offset += entry.positions.length;
    }
  } finally {
    fs.closeSync(fd);
  }
}

// 将 3MF 网格转换为 OBJ 文件。
async function convert3mfToObj(sourcePath, targetPath) {
  const buffer = fs.readFileSync(sourcePath);
  const zip = await JSZip.loadAsync(buffer);
  const modelFile = zip.file(/3dmodel\.model$/i)[0] || zip.file(/\.model$/i)[0];
  if (!modelFile) {
    throw new Error("3MF file does not contain 3D model data");
  }

  const xmlText = await modelFile.async("text");
  const parser = new XMLParser({
    ignoreAttributes: false,
    attributeNamePrefix: "@_",
    removeNSPrefix: true
  });
  const xml = parser.parse(xmlText);
  const model = xml.model;
  const meshEntries = [];
  await collectMeshesFromModel(zip, model, meshEntries);

  if (meshEntries.length === 0) {
    throw new Error("3MF file does not contain mesh geometry");
  }

  writeObjFromMeshes(meshEntries, targetPath);
}

ensureDirs();

app.use(express.json());
app.use(express.static(path.join(__dirname, "public")));
app.use("/vendor/three", express.static(path.join(__dirname, "node_modules", "three")));
app.use("/files", express.static(workRoot));

app.get("/api/health", (_request, response) => {
  response.json({
    ok: true,
    toolExists: fs.existsSync(toolPath)
  });
});

app.post("/api/lod", upload.single("model"), async (request, response) => {
  try {
    if (!request.file) {
      response.status(400).json({ error: "missing OBJ file" });
      return;
    }

    const ext = path.extname(request.file.originalname).toLowerCase();
    if (ext !== ".obj" && ext !== ".3mf") {
      response.status(400).json({ error: "only OBJ and 3MF upload are supported in this version" });
      return;
    }

    if (!fs.existsSync(toolPath)) {
      response.status(500).json({ error: `missing tool: ${toolPath}` });
      return;
    }

    const hash = fileHash(request.file.buffer);
    const materialFilePath = findMatchingMaterialFile(request.file.originalname, hash);
    const jobIndex = readJobIndex();
    const cachedJob = jobIndex[hash];
    if (cachedJobReady(cachedJob)) {
      if (ext === ".3mf" && !cachedJob.materialObjPath) {
        cachedJob.materialObjPath = archiveConvertedObjToMaterial(cachedJob.sourceObjPath, materialFilePath);
        jobIndex[hash] = cachedJob;
        writeJobIndex(jobIndex);
      }
      cleanupUploadRoot();
      response.json(jobResponse(cachedJob, request.file.originalname, true));
      return;
    }
    if (cachedJob) {
      delete jobIndex[hash];
      writeJobIndex(jobIndex);
    }

    const jobName = `${safeBaseName(request.file.originalname)}_${hash.slice(0, 12)}`;
    const rawInputPath = path.join(uploadRoot, `${jobName}${ext}`);
    const outputDir = path.join(modelRoot, jobName);
    const sourceObjPath = path.join(outputDir, "source.obj");
    const statsPath = path.join(tableRoot, `${jobName}_stats.csv`);

    try {
      fs.mkdirSync(outputDir, { recursive: true });
      if (ext === ".obj") {
        fs.writeFileSync(sourceObjPath, request.file.buffer);
      } else {
        fs.writeFileSync(rawInputPath, request.file.buffer);
        await convert3mfToObj(rawInputPath, sourceObjPath);
      }

      await runCommand(toolPath, [
        "generate-lod",
        toolRelative(sourceObjPath),
        toolRelative(outputDir),
        toolRelative(statsPath),
        "--backend",
        "meshopt"
      ]);
    } catch (error) {
      removeIfExists(rawInputPath);
      cleanupUploadRoot();
      if (fs.existsSync(outputDir)) {
        fs.rmSync(outputDir, { recursive: true, force: true });
      }
      removeIfExists(statsPath);
      throw error;
    }

    const job = {
      name: jobName,
      hash,
      originalName: request.file.originalname,
      outputDir,
      sourceObjPath,
      statsPath,
      materialObjPath: ext === ".3mf" ? archiveConvertedObjToMaterial(sourceObjPath, materialFilePath) : ""
    };
    jobIndex[hash] = job;
    writeJobIndex(jobIndex);
    removeIfExists(rawInputPath);
    cleanupUploadRoot();

    response.json(jobResponse(job, request.file.originalname, false));
  } catch (error) {
    cleanupUploadRoot();
    response.status(500).json({ error: error.message });
  }
});

const port = Number(process.env.PORT || 5177);
app.listen(port, () => {
  console.log(`Mesh LOD Web running at http://localhost:${port}`);
});
