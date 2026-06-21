// Viewer.cpp
// 实现基于 Win32 和 OpenGL 的网格查看窗口。

#include "viewer/Viewer.h"

#ifdef _WIN32

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <windowsx.h>
#include <GL/gl.h>

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>
#include <vector>

namespace viewer {
namespace {

constexpr double kEpsilon = 1e-12;

struct PreparedModel {
    // name 表示当前模型的显示名称。
    std::string name;
    // mesh 是用于绘制的三维网格。
    mesh::Mesh mesh;
    // center/radius 用于把模型放到视野中心。
    mesh::Vec3 center;
    double radius = 1.0;
};

struct ViewerState {
    // models 存储可切换显示的模型列表。
    std::vector<PreparedModel> models;
    // active_model 表示当前显示的模型下标。
    std::size_t active_model = 0;
    // device_context 是 OpenGL 绘制使用的窗口 DC。
    HDC device_context = nullptr;
    // gl_context 是当前窗口的 OpenGL 渲染上下文。
    HGLRC gl_context = nullptr;
    int width = 1280;
    int height = 720;
    double rotation_x = 20.0;
    double rotation_y = -35.0;
    double zoom = 1.0;
    bool dragging = false;
    bool wireframe = false;
    bool auto_lod = false;
    POINT last_mouse{};
};

// 计算两个三维坐标之差。
mesh::Vec3 subtract(const mesh::Vec3& lhs, const mesh::Vec3& rhs) {
    return {lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z};
}

// 计算两个三维向量的叉积。
mesh::Vec3 cross(const mesh::Vec3& lhs, const mesh::Vec3& rhs) {
    return {
        lhs.y * rhs.z - lhs.z * rhs.y,
        lhs.z * rhs.x - lhs.x * rhs.z,
        lhs.x * rhs.y - lhs.y * rhs.x
    };
}

// 计算三维向量长度。
double length(const mesh::Vec3& value) {
    return std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z);
}

// 计算模型的中心点。
mesh::Vec3 boundsCenter(const mesh::Bounds& bounds) {
    return {
        (bounds.min.x + bounds.max.x) * 0.5,
        (bounds.min.y + bounds.max.y) * 0.5,
        (bounds.min.z + bounds.max.z) * 0.5
    };
}

// 计算模型包围盒半径。
double boundsRadius(const mesh::Bounds& bounds, const mesh::Vec3& center) {
    const std::vector<mesh::Vec3> corners = {
        {bounds.min.x, bounds.min.y, bounds.min.z},
        {bounds.min.x, bounds.min.y, bounds.max.z},
        {bounds.min.x, bounds.max.y, bounds.min.z},
        {bounds.min.x, bounds.max.y, bounds.max.z},
        {bounds.max.x, bounds.min.y, bounds.min.z},
        {bounds.max.x, bounds.min.y, bounds.max.z},
        {bounds.max.x, bounds.max.y, bounds.min.z},
        {bounds.max.x, bounds.max.y, bounds.max.z},
    };

    double radius = 1.0;
    for (const mesh::Vec3& corner : corners) {
        radius = std::max(radius, length(subtract(corner, center)));
    }
    return radius;
}

// 准备模型显示所需的中心点和缩放半径。
PreparedModel prepareModel(const ViewerModel& input, const mesh::Vec3& center, double radius) {
    PreparedModel output;
    output.name = input.name;
    output.mesh = input.mesh;
    output.center = center;
    output.radius = radius;
    return output;
}

// 计算模型组共用的显示范围。
PreparedModel prepareReferenceModel(const ViewerModel& input) {
    PreparedModel output;
    output.name = input.name;
    output.mesh = input.mesh;

    const mesh::Bounds bounds = mesh::computeBounds(input.mesh);
    if (bounds.valid) {
        output.center = boundsCenter(bounds);
        output.radius = boundsRadius(bounds, output.center);
    }

    return output;
}

// 设置 OpenGL 透视投影矩阵。
void setPerspective(double fovy_degrees, double aspect, double near_plane, double far_plane) {
    const double radians = fovy_degrees * 3.14159265358979323846 / 180.0;
    const double top = std::tan(radians * 0.5) * near_plane;
    const double right = top * aspect;
    glFrustum(-right, right, -top, top, near_plane, far_plane);
}

// 配置相机空间光源和材质。
void configureLighting() {
    const float scene_ambient[] = {0.22f, 0.24f, 0.27f, 1.0f};
    const float main_position[] = {-0.35f, 0.55f, 0.75f, 0.0f};
    const float main_diffuse[] = {0.58f, 0.58f, 0.56f, 1.0f};
    const float main_specular[] = {0.10f, 0.10f, 0.10f, 1.0f};
    const float fill_position[] = {0.45f, -0.35f, 0.45f, 0.0f};
    const float fill_diffuse[] = {0.22f, 0.24f, 0.28f, 1.0f};
    const float fill_specular[] = {0.0f, 0.0f, 0.0f, 1.0f};
    const float material_specular[] = {0.08f, 0.08f, 0.08f, 1.0f};
    const float material_shininess[] = {18.0f};

    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_LIGHT1);
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, scene_ambient);
    glLightModeli(GL_LIGHT_MODEL_LOCAL_VIEWER, GL_TRUE);

    glLightfv(GL_LIGHT0, GL_POSITION, main_position);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, main_diffuse);
    glLightfv(GL_LIGHT0, GL_SPECULAR, main_specular);

    glLightfv(GL_LIGHT1, GL_POSITION, fill_position);
    glLightfv(GL_LIGHT1, GL_DIFFUSE, fill_diffuse);
    glLightfv(GL_LIGHT1, GL_SPECULAR, fill_specular);

    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, material_specular);
    glMaterialfv(GL_FRONT_AND_BACK, GL_SHININESS, material_shininess);
}

// 更新窗口标题。
void updateWindowTitle(HWND hwnd, const ViewerState& state) {
    const PreparedModel& model = state.models[state.active_model];
    const std::string mode = state.wireframe ? "wireframe" : "solid";
    const std::string lod_mode = state.auto_lod ? "auto" : "manual";
    const std::string lod_index = std::to_string(state.active_model + 1) + "/" + std::to_string(state.models.size());
    const std::string title = "Mesh LOD Viewer - " + model.name
        + " - LOD " + lod_index
        + " - " + lod_mode
        + " - " + mode
        + " - " + std::to_string(model.mesh.faces.size()) + " triangles";
    SetWindowTextA(hwnd, title.c_str());
}

// 根据缩放距离选择自动 LOD 层级。
std::size_t autoLodIndex(const ViewerState& state) {
    if (state.models.size() <= 1 || state.zoom <= 1.0) {
        return 0;
    }

    constexpr double kNearZoom = 1.0;
    constexpr double kFarZoom = 6.0;
    const double t = std::clamp((state.zoom - kNearZoom) / (kFarZoom - kNearZoom), 0.0, 1.0);
    const auto index = static_cast<std::size_t>(std::floor(t * static_cast<double>(state.models.size())));
    return std::min(index, state.models.size() - 1);
}

// 同步自动 LOD 当前层级。
void updateAutoLod(HWND hwnd, ViewerState& state) {
    if (!state.auto_lod) {
        return;
    }

    const std::size_t next_model = autoLodIndex(state);
    if (next_model != state.active_model) {
        state.active_model = next_model;
    }
    updateWindowTitle(hwnd, state);
}

// 设置当前显示的 LOD 层级。
void setActiveModel(HWND hwnd, ViewerState& state, std::size_t index, bool manual) {
    if (index >= state.models.size()) {
        return;
    }

    state.active_model = index;
    if (manual) {
        state.auto_lod = false;
    }
    updateWindowTitle(hwnd, state);
    InvalidateRect(hwnd, nullptr, FALSE);
}

// 重置相机状态。
void resetCamera(ViewerState& state) {
    state.rotation_x = 20.0;
    state.rotation_y = -35.0;
    state.zoom = 1.0;
}

// 绘制一个三角网格。
void drawMesh(const PreparedModel& model) {
    glBegin(GL_TRIANGLES);
    for (const mesh::Face& face : model.mesh.faces) {
        const mesh::Vec3& p0 = model.mesh.positions[face.vertex_indices[0]];
        const mesh::Vec3& p1 = model.mesh.positions[face.vertex_indices[1]];
        const mesh::Vec3& p2 = model.mesh.positions[face.vertex_indices[2]];
        mesh::Vec3 normal = cross(subtract(p1, p0), subtract(p2, p0));
        const double normal_length = length(normal);
        if (normal_length > kEpsilon) {
            normal.x /= normal_length;
            normal.y /= normal_length;
            normal.z /= normal_length;
            glNormal3d(normal.x, normal.y, normal.z);
        }

        glVertex3d(p0.x, p0.y, p0.z);
        glVertex3d(p1.x, p1.y, p1.z);
        glVertex3d(p2.x, p2.y, p2.z);
    }
    glEnd();
}

// 绘制坐标轴。
void drawAxes() {
    glDisable(GL_LIGHTING);
    glLineWidth(2.0f);
    glBegin(GL_LINES);
    glColor3d(1.0, 0.1, 0.1);
    glVertex3d(0.0, 0.0, 0.0);
    glVertex3d(1.2, 0.0, 0.0);
    glColor3d(0.1, 0.8, 0.1);
    glVertex3d(0.0, 0.0, 0.0);
    glVertex3d(0.0, 1.2, 0.0);
    glColor3d(0.1, 0.3, 1.0);
    glVertex3d(0.0, 0.0, 0.0);
    glVertex3d(0.0, 0.0, 1.2);
    glEnd();
    glLineWidth(1.0f);
    glEnable(GL_LIGHTING);
}

// 渲染当前帧。
void render(HWND hwnd, HDC device_context, const ViewerState& state) {
    const PreparedModel& model = state.models[state.active_model];

    glViewport(0, 0, state.width, state.height);
    glClearColor(0.10f, 0.11f, 0.13f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_NORMALIZE);
    glDisable(GL_CULL_FACE);
    glShadeModel(GL_FLAT);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    const double aspect = state.height > 0 ? static_cast<double>(state.width) / state.height : 1.0;
    setPerspective(45.0, aspect, 0.1, 100.0);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    configureLighting();
    glTranslated(0.0, 0.0, -3.5 * state.zoom);
    glRotated(state.rotation_x, 1.0, 0.0, 0.0);
    glRotated(state.rotation_y, 0.0, 1.0, 0.0);

    drawAxes();

    const double scale = model.radius > kEpsilon ? 1.0 / model.radius : 1.0;
    glScaled(scale, scale, scale);
    glTranslated(-model.center.x, -model.center.y, -model.center.z);

    glColor3d(0.58, 0.64, 0.70);
    glPolygonMode(GL_FRONT_AND_BACK, state.wireframe ? GL_LINE : GL_FILL);
    drawMesh(model);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    SwapBuffers(device_context);
    ValidateRect(hwnd, nullptr);
}

// 创建 OpenGL 渲染上下文。
HGLRC createOpenGlContext(HDC device_context) {
    PIXELFORMATDESCRIPTOR pfd{};
    pfd.nSize = sizeof(PIXELFORMATDESCRIPTOR);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cDepthBits = 24;
    pfd.iLayerType = PFD_MAIN_PLANE;

    const int pixel_format = ChoosePixelFormat(device_context, &pfd);
    if (pixel_format == 0) {
        throw std::runtime_error("failed to choose OpenGL pixel format");
    }

    if (!SetPixelFormat(device_context, pixel_format, &pfd)) {
        throw std::runtime_error("failed to set OpenGL pixel format");
    }

    HGLRC context = wglCreateContext(device_context);
    if (!context) {
        throw std::runtime_error("failed to create OpenGL context");
    }

    if (!wglMakeCurrent(device_context, context)) {
        wglDeleteContext(context);
        throw std::runtime_error("failed to activate OpenGL context");
    }

    return context;
}

// 处理窗口消息。
LRESULT CALLBACK windowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    ViewerState* state = reinterpret_cast<ViewerState*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));

    switch (message) {
        case WM_SIZE:
            if (state) {
                state->width = LOWORD(lparam);
                state->height = HIWORD(lparam);
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;

        case WM_LBUTTONDOWN:
            if (state) {
                state->dragging = true;
                state->last_mouse.x = GET_X_LPARAM(lparam);
                state->last_mouse.y = GET_Y_LPARAM(lparam);
                SetCapture(hwnd);
            }
            return 0;

        case WM_LBUTTONUP:
            if (state) {
                state->dragging = false;
                ReleaseCapture();
            }
            return 0;

        case WM_MOUSEMOVE:
            if (state && state->dragging) {
                const POINT current{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
                state->rotation_y += static_cast<double>(current.x - state->last_mouse.x) * 0.4;
                state->rotation_x += static_cast<double>(current.y - state->last_mouse.y) * 0.4;
                state->last_mouse = current;
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;

        case WM_MOUSEWHEEL:
            if (state) {
                const int delta = GET_WHEEL_DELTA_WPARAM(wparam);
                state->zoom *= delta > 0 ? 0.9 : 1.1;
                state->zoom = std::clamp(state->zoom, 0.2, 10.0);
                updateAutoLod(hwnd, *state);
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;

        case WM_KEYDOWN:
            if (state) {
                if (wparam >= '1' && wparam <= '9') {
                    const std::size_t index = static_cast<std::size_t>(wparam - '1');
                    setActiveModel(hwnd, *state, index, true);
                } else if (wparam == '0') {
                    setActiveModel(hwnd, *state, 9, true);
                } else if (wparam == VK_OEM_4) {
                    if (state->active_model > 0) {
                        setActiveModel(hwnd, *state, state->active_model - 1, true);
                    }
                } else if (wparam == VK_OEM_6) {
                    setActiveModel(hwnd, *state, state->active_model + 1, true);
                } else if (wparam == 'A') {
                    state->auto_lod = !state->auto_lod;
                    updateAutoLod(hwnd, *state);
                    updateWindowTitle(hwnd, *state);
                    InvalidateRect(hwnd, nullptr, FALSE);
                } else if (wparam == 'W') {
                    state->wireframe = !state->wireframe;
                    updateWindowTitle(hwnd, *state);
                    InvalidateRect(hwnd, nullptr, FALSE);
                } else if (wparam == 'R') {
                    resetCamera(*state);
                    updateAutoLod(hwnd, *state);
                    InvalidateRect(hwnd, nullptr, FALSE);
                } else if (wparam == VK_ESCAPE) {
                    DestroyWindow(hwnd);
                }
            }
            return 0;

        case WM_PAINT:
            if (state) {
                PAINTSTRUCT ps;
                BeginPaint(hwnd, &ps);
                render(hwnd, state->device_context, *state);
                EndPaint(hwnd, &ps);
            }
            return 0;

        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;

        default:
            return DefWindowProc(hwnd, message, wparam, lparam);
    }
}

// 注册 viewer 窗口类。
ATOM registerWindowClass(HINSTANCE instance) {
    WNDCLASSA wc{};
    wc.style = CS_OWNDC;
    wc.lpfnWndProc = windowProc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = "MeshLodViewerWindow";
    return RegisterClassA(&wc);
}

}  // namespace

int runViewer(const std::vector<ViewerModel>& models, bool auto_lod) {
    if (models.empty()) {
        throw std::runtime_error("viewer requires at least one model");
    }

    ViewerState state;
    state.auto_lod = auto_lod && models.size() > 1;
    const PreparedModel reference = prepareReferenceModel(models.front());
    for (const ViewerModel& model : models) {
        state.models.push_back(prepareModel(model, reference.center, reference.radius));
    }

    HINSTANCE instance = GetModuleHandle(nullptr);
    if (!registerWindowClass(instance)) {
        throw std::runtime_error("failed to register viewer window class");
    }

    HWND hwnd = CreateWindowExA(
        0,
        "MeshLodViewerWindow",
        "Mesh LOD Viewer",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        state.width,
        state.height,
        nullptr,
        nullptr,
        instance,
        nullptr
    );

    if (!hwnd) {
        throw std::runtime_error("failed to create viewer window");
    }

    SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(&state));
    updateWindowTitle(hwnd, state);

    state.device_context = GetDC(hwnd);
    state.gl_context = createOpenGlContext(state.device_context);
    InvalidateRect(hwnd, nullptr, FALSE);

    MSG message{};
    while (GetMessage(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessage(&message);
    }

    wglMakeCurrent(nullptr, nullptr);
    wglDeleteContext(state.gl_context);
    ReleaseDC(hwnd, state.device_context);

    return 0;
}

}  // namespace viewer

#else

#include <stdexcept>

namespace viewer {

int runViewer(const std::vector<ViewerModel>&, bool) {
    throw std::runtime_error("viewer is only implemented on Windows");
}

}  // namespace viewer

#endif
