// main.cpp
// 提供命令行入口，负责模型信息查看、OBJ 复制、网格简化和模型查看。

#include <chrono>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "mesh/Mesh.h"
#include "mesh/ObjIO.h"
#include "mesh/PrimitiveMeshes.h"
#include "mesh/Simplifier.h"
#include "viewer/Viewer.h"

namespace {

struct SimplifyCommand {
    // input_path 是待简化的 OBJ 文件路径。
    std::filesystem::path input_path;
    // output_path 是简化后 OBJ 文件路径。
    std::filesystem::path output_path;
    // ratio 表示目标三角面比例。
    double ratio = 1.0;
    // backend 表示网格简化后端。
    mesh::SimplifyBackend backend = mesh::SimplifyBackend::MeshOptimizer;
    // preserve_boundary 控制是否保护边界顶点。
    bool preserve_boundary = true;
    // prevent_normal_flip 控制是否启用法线翻转保护。
    bool prevent_normal_flip = true;
};

struct LodLevel {
    // name 表示 LOD 文件名中的级别标记。
    std::string name;
    // ratio 表示目标三角面比例。
    double ratio = 1.0;
};

// 生成默认的多级 LOD 列表。
std::vector<LodLevel> defaultLodLevels() {
    return {
        {"100", 1.0},
        {"095", 0.95},
        {"090", 0.90},
        {"085", 0.85},
        {"080", 0.80},
        {"075", 0.75},
        {"070", 0.70},
        {"065", 0.65},
        {"060", 0.60},
        {"055", 0.55},
        {"050", 0.50},
        {"045", 0.45},
        {"040", 0.40},
        {"035", 0.35},
        {"030", 0.30},
        {"025", 0.25},
        {"020", 0.20},
        {"018", 0.18},
        {"016", 0.16},
        {"014", 0.14},
        {"012", 0.12},
        {"010", 0.10},
        {"008", 0.08},
        {"006", 0.06},
        {"004", 0.04},
        {"003", 0.03},
        {"002", 0.02},
        {"001", 0.01},
    };
}

struct GenerateLodCommand {
    // input_path 是用于生成 LOD 的原始 OBJ 文件。
    std::filesystem::path input_path;
    // output_dir 是多级 LOD 模型输出目录。
    std::filesystem::path output_dir;
    // stats_path 是 CSV 统计表输出路径。
    std::filesystem::path stats_path;
    // backend 表示网格简化后端。
    mesh::SimplifyBackend backend = mesh::SimplifyBackend::MeshOptimizer;
    // preserve_boundary 控制是否保护边界顶点。
    bool preserve_boundary = true;
    // prevent_normal_flip 控制是否启用法线翻转保护。
    bool prevent_normal_flip = true;
};

// 打印命令行使用方式。
void printUsage(const char* program_name) {
    std::cerr << "Usage:\n"
              << "  " << program_name << " info <input.obj>\n"
              << "  " << program_name << " copy <input.obj> <output.obj>\n"
              << "  " << program_name << " create-compound <output.obj>\n"
              << "  " << program_name << " generate-lod <input.obj> <output-dir> <stats.csv> [--backend meshopt|qem] [--no-boundary] [--allow-flip]\n"
              << "  " << program_name << " simplify <input.obj> <output.obj> <ratio> [--backend meshopt|qem] [--no-boundary] [--allow-flip]\n"
              << "  " << program_name << " view <input.obj> [compare.obj ...]\n"
              << "  " << program_name << " view-lod <lod.obj> <lod.obj> ...\n";
}

// 打印模型的基础统计信息。
void printMeshInfo(const mesh::Mesh& model) {
    const mesh::Bounds bounds = mesh::computeBounds(model);

    std::cout << "Vertices: " << model.positions.size() << '\n';
    std::cout << "Triangles: " << model.faces.size() << '\n';

    if (bounds.valid) {
        std::cout << "Bounds min: "
                  << bounds.min.x << ", " << bounds.min.y << ", " << bounds.min.z << '\n';
        std::cout << "Bounds max: "
                  << bounds.max.x << ", " << bounds.max.y << ", " << bounds.max.z << '\n';
    }
}

// 解析浮点比例参数。
double parseRatio(const std::string& text) {
    std::size_t parsed_chars = 0;
    const double ratio = std::stod(text, &parsed_chars);
    if (parsed_chars != text.size()) {
        throw std::runtime_error("ratio contains invalid characters: " + text);
    }
    return ratio;
}

// 将简化后端转换为命令行显示文本。
std::string backendName(mesh::SimplifyBackend backend) {
    switch (backend) {
        case mesh::SimplifyBackend::Qem:
            return "qem";
        case mesh::SimplifyBackend::MeshOptimizer:
            return "meshopt";
    }
    return "unknown";
}

// 解析简化后端参数。
mesh::SimplifyBackend parseBackend(const std::string& text) {
    if (text == "qem") {
        return mesh::SimplifyBackend::Qem;
    }
    if (text == "meshopt" || text == "meshoptimizer") {
        return mesh::SimplifyBackend::MeshOptimizer;
    }
    throw std::runtime_error("unknown backend: " + text);
}

// 解析通用简化选项。
void parseSimplifyOption(
    const std::string& option,
    int& index,
    int argc,
    char** argv,
    mesh::SimplifyBackend& backend,
    bool& preserve_boundary,
    bool& prevent_normal_flip
) {
    constexpr const char* kBackendPrefix = "--backend=";

    if (option == "--no-boundary") {
        preserve_boundary = false;
    } else if (option == "--allow-flip") {
        prevent_normal_flip = false;
    } else if (option == "--backend") {
        if (index + 1 >= argc) {
            throw std::runtime_error("--backend requires qem or meshopt");
        }
        backend = parseBackend(argv[++index]);
    } else if (option.rfind(kBackendPrefix, 0) == 0) {
        backend = parseBackend(option.substr(std::string(kBackendPrefix).size()));
    } else {
        throw std::runtime_error("unknown option: " + option);
    }
}

// 解析 simplify 子命令参数。
SimplifyCommand parseSimplifyCommand(int argc, char** argv) {
    if (argc < 5) {
        throw std::runtime_error("simplify requires input, output and ratio");
    }

    SimplifyCommand command;
    command.input_path = argv[2];
    command.output_path = argv[3];
    command.ratio = parseRatio(argv[4]);

    for (int i = 5; i < argc; ++i) {
        const std::string option = argv[i];
        parseSimplifyOption(
            option,
            i,
            argc,
            argv,
            command.backend,
            command.preserve_boundary,
            command.prevent_normal_flip
        );
    }

    return command;
}

// 解析 generate-lod 子命令参数。
GenerateLodCommand parseGenerateLodCommand(int argc, char** argv) {
    if (argc < 5) {
        throw std::runtime_error("generate-lod requires input path, output directory and stats CSV path");
    }

    GenerateLodCommand command;
    command.input_path = argv[2];
    command.output_dir = argv[3];
    command.stats_path = argv[4];

    for (int i = 5; i < argc; ++i) {
        const std::string option = argv[i];
        parseSimplifyOption(
            option,
            i,
            argc,
            argv,
            command.backend,
            command.preserve_boundary,
            command.prevent_normal_flip
        );
    }

    return command;
}

// 按指定后端执行网格简化。
mesh::SimplifyResult simplifyWithBackend(
    const mesh::Mesh& input,
    const mesh::SimplifyOptions& options,
    mesh::SimplifyBackend backend
) {
    switch (backend) {
        case mesh::SimplifyBackend::Qem:
            return mesh::simplifyQem(input, options);
        case mesh::SimplifyBackend::MeshOptimizer:
            return mesh::simplifyMeshOptimizer(input, options);
    }
    throw std::runtime_error("unsupported simplify backend");
}

// 执行模型信息查看。
int runInfo(const std::filesystem::path& input_path) {
    const mesh::Mesh model = mesh::loadObj(input_path);
    printMeshInfo(model);
    return 0;
}

// 执行 OBJ 复制。
int runCopy(const std::filesystem::path& input_path, const std::filesystem::path& output_path) {
    const mesh::Mesh model = mesh::loadObj(input_path);
    printMeshInfo(model);
    mesh::saveObj(model, output_path);
    std::cout << "Saved: " << output_path.string() << '\n';
    return 0;
}

// 执行组合几何模型生成。
int runCreateCompound(const std::filesystem::path& output_path) {
    std::filesystem::create_directories(output_path.parent_path());
    const mesh::Mesh model = mesh::createCompoundMesh();
    mesh::saveObj(model, output_path);
    printMeshInfo(model);
    std::cout << "Saved: " << output_path.string() << '\n';
    return 0;
}

// 执行网格简化。
int runSimplify(const SimplifyCommand& command) {
    const auto load_start = std::chrono::steady_clock::now();
    const mesh::Mesh input = mesh::loadObj(command.input_path);
    const auto load_end = std::chrono::steady_clock::now();

    mesh::SimplifyOptions options;
    options.target_faces = mesh::targetFaceCountFromRatio(input.faces.size(), command.ratio);
    options.preserve_boundary = command.preserve_boundary;
    options.prevent_normal_flip = command.prevent_normal_flip;

    const auto simplify_start = std::chrono::steady_clock::now();
    const mesh::SimplifyResult result = simplifyWithBackend(input, options, command.backend);
    const auto simplify_end = std::chrono::steady_clock::now();

    mesh::saveObj(result.mesh, command.output_path);

    const auto load_ms = std::chrono::duration_cast<std::chrono::milliseconds>(load_end - load_start).count();
    const auto simplify_ms = std::chrono::duration_cast<std::chrono::milliseconds>(simplify_end - simplify_start).count();

    std::cout << "Backend: " << backendName(command.backend) << '\n';
    std::cout << "Input vertices: " << result.stats.input_vertices << '\n';
    std::cout << "Input triangles: " << result.stats.input_faces << '\n';
    std::cout << "Target triangles: " << options.target_faces << '\n';
    std::cout << "Output vertices: " << result.stats.output_vertices << '\n';
    std::cout << "Output triangles: " << result.stats.output_faces << '\n';
    std::cout << "Collapses: " << result.stats.collapses << '\n';
    std::cout << "Boundary rejections: " << result.stats.boundary_rejections << '\n';
    std::cout << "Normal flip rejections: " << result.stats.normal_flip_rejections << '\n';
    std::cout << "Normalized error: " << result.stats.normalized_error << '\n';
    std::cout << "Absolute error: " << result.stats.absolute_error << '\n';
    std::cout << "Reached target: " << (result.stats.reached_target ? "yes" : "no") << '\n';
    std::cout << "Load time: " << load_ms << " ms\n";
    std::cout << "Simplify time: " << simplify_ms << " ms\n";
    std::cout << "Saved: " << command.output_path.string() << '\n';

    return 0;
}

// 写入一行 LOD 统计结果。
void writeLodStatsRow(
    std::ofstream& output,
    const std::string& level,
    const std::filesystem::path& path,
    const std::string& backend,
    double ratio,
    const mesh::SimplifyStats& stats,
    long long simplify_ms
) {
    output << level << ','
           << path.string() << ','
           << backend << ','
           << ratio << ','
           << stats.input_vertices << ','
           << stats.input_faces << ','
           << stats.output_vertices << ','
           << stats.output_faces << ','
           << stats.collapses << ','
           << stats.boundary_rejections << ','
           << stats.normal_flip_rejections << ','
           << stats.normalized_error << ','
           << stats.absolute_error << ','
           << (stats.reached_target ? "yes" : "no") << ','
           << simplify_ms << '\n';
}

// 执行多级 LOD 生成和统计输出。
int runGenerateLod(const GenerateLodCommand& command) {
    const std::vector<LodLevel> levels = defaultLodLevels();

    std::filesystem::create_directories(command.output_dir);
    std::filesystem::create_directories(command.stats_path.parent_path());

    const mesh::Mesh input = mesh::loadObj(command.input_path);
    std::ofstream stats_output(command.stats_path);
    if (!stats_output) {
        throw std::runtime_error("failed to write stats CSV: " + command.stats_path.string());
    }

    const std::string backend = backendName(command.backend);
    stats_output << "level,file,backend,ratio,input_vertices,input_faces,output_vertices,output_faces,collapses,boundary_rejections,normal_flip_rejections,normalized_error,absolute_error,reached_target,simplify_ms\n";

    for (const LodLevel& level : levels) {
        const std::filesystem::path output_path = command.output_dir / ("lod_" + level.name + ".obj");

        if (level.ratio == 1.0) {
            mesh::saveObj(input, output_path);

            mesh::SimplifyStats stats;
            stats.input_vertices = input.positions.size();
            stats.input_faces = input.faces.size();
            stats.output_vertices = input.positions.size();
            stats.output_faces = input.faces.size();
            stats.reached_target = true;

            writeLodStatsRow(stats_output, level.name, output_path, backend, level.ratio, stats, 0);
            std::cout << "Generated LOD " << level.name << ": " << output_path.string() << '\n';
            continue;
        }

        mesh::SimplifyOptions options;
        options.target_faces = mesh::targetFaceCountFromRatio(input.faces.size(), level.ratio);
        options.preserve_boundary = command.preserve_boundary;
        options.prevent_normal_flip = command.prevent_normal_flip;

        const auto start = std::chrono::steady_clock::now();
        const mesh::SimplifyResult result = simplifyWithBackend(input, options, command.backend);
        const auto end = std::chrono::steady_clock::now();
        const auto simplify_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

        mesh::saveObj(result.mesh, output_path);
        writeLodStatsRow(stats_output, level.name, output_path, backend, level.ratio, result.stats, simplify_ms);
        std::cout << "Generated LOD " << level.name << ": " << output_path.string() << '\n';
    }

    std::cout << "Saved stats: " << command.stats_path.string() << '\n';
    return 0;
}

// 执行 OpenGL 模型查看。
int runView(int argc, char** argv, bool auto_lod) {
    if (argc < 3) {
        throw std::runtime_error("view requires at least one input path");
    }

    std::vector<viewer::ViewerModel> models;
    for (int i = 2; i < argc; ++i) {
        const std::filesystem::path path = argv[i];
        viewer::ViewerModel model;
        model.name = path.filename().string();
        model.mesh = mesh::loadObj(path);
        models.push_back(model);
    }

    return viewer::runViewer(models, auto_lod);
}

}  // namespace

// 程序入口。
int main(int argc, char** argv) {
    if (argc < 2) {
        printUsage(argv[0]);
        return 1;
    }

    try {
        const std::string command = argv[1];

        if (command == "info") {
            if (argc != 3) {
                throw std::runtime_error("info requires exactly one input path");
            }
            return runInfo(argv[2]);
        }

        if (command == "copy") {
            if (argc != 4) {
                throw std::runtime_error("copy requires input and output paths");
            }
            return runCopy(argv[2], argv[3]);
        }

        if (command == "create-compound") {
            if (argc != 3) {
                throw std::runtime_error("create-compound requires one output path");
            }
            return runCreateCompound(argv[2]);
        }

        if (command == "generate-lod") {
            return runGenerateLod(parseGenerateLodCommand(argc, argv));
        }

        if (command == "simplify") {
            return runSimplify(parseSimplifyCommand(argc, argv));
        }

        if (command == "view") {
            return runView(argc, argv, false);
        }

        if (command == "view-lod") {
            if (argc < 4) {
                throw std::runtime_error("view-lod requires at least two LOD model paths");
            }
            return runView(argc, argv, true);
        }

        throw std::runtime_error("unknown command: " + command);
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
        printUsage(argv[0]);
        return 1;
    }
}
