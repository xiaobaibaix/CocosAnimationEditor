# CMakeLists.txt - 外部 CMake 封装，编译 Dear ImGui 核心库
# 不修改 Library/imgui/imgui/ 内的任何文件
cmake_minimum_required(VERSION 3.6)

set(IMGUI_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/imgui)

# Dear ImGui 核心源文件（不含 backend，由平台层单独处理）
set(IMGUI_SOURCES
    ${IMGUI_ROOT}/imgui.cpp
    ${IMGUI_ROOT}/imgui_demo.cpp
    ${IMGUI_ROOT}/imgui_draw.cpp
    ${IMGUI_ROOT}/imgui_widgets.cpp
    ${IMGUI_ROOT}/imgui_tables.cpp
)

add_library(imgui_core STATIC ${IMGUI_SOURCES})

target_include_directories(imgui_core PUBLIC ${IMGUI_ROOT})

# imgui 不需要 DLL 导出宏
target_compile_definitions(imgui_core PUBLIC IMGUI_STATIC)

# 忽略第三方代码的警告
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(imgui_core PRIVATE
        -Wno-deprecated-declarations
        -Wno-unused-parameter
    )
endif()
