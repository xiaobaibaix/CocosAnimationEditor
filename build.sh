#!/bin/bash
set -e

BUILD_DIR="$(dirname "$0")/proj.linux/build"

# 首次运行时配置 CMake
if [ ! -f "$BUILD_DIR/Makefile" ]; then
    echo "==> 首次构建，正在配置 CMake..."
    mkdir -p "$BUILD_DIR"
    cmake -S "$(dirname "$0")" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Debug
fi

# 编译
cmake --build "$BUILD_DIR" -- -j$(nproc)

echo "==> 编译完成"
echo "==> 可执行文件: $BUILD_DIR/bin/CocosAnimationEditor/CocosAnimationEditor"
