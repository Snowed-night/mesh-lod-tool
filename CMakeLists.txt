# CMakeLists.txt
# 配置项目目标、源码文件和编译选项。

cmake_minimum_required(VERSION 3.16)

project(MeshLodTool LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(mesh_core
    src/mesh/ObjIO.cpp
    src/mesh/PrimitiveMeshes.cpp
    src/mesh/Simplifier.cpp
    src/mesh/MeshOptimizerSimplifier.cpp
    third_party/meshoptimizer/src/allocator.cpp
    third_party/meshoptimizer/src/simplifier.cpp
)

target_include_directories(mesh_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/meshoptimizer/src
)

if(MSVC)
    target_compile_options(mesh_core PRIVATE /W4 /permissive-)
else()
    target_compile_options(mesh_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(mesh_lod_tool
    src/main.cpp
    src/viewer/Viewer.cpp
)

target_link_libraries(mesh_lod_tool
    PRIVATE
        mesh_core
)

if(WIN32)
    target_link_libraries(mesh_lod_tool
        PRIVATE
            opengl32
            gdi32
    )
endif()

if(MSVC)
    target_compile_options(mesh_lod_tool PRIVATE /W4 /permissive-)
else()
    target_compile_options(mesh_lod_tool PRIVATE -Wall -Wextra -Wpedantic)
endif()
