# CMakeLists.txt - 外部 CMake 封装，编译 UGF 框架库
# 不修改 Library/ugf/universal_game_framework/ 内的任何文件
# UGF 自带的 CMakeLists.txt 支持作为子项目集成，这里只做配置代理

# UGF 内部需要 3.15，在此声明
cmake_minimum_required(VERSION 3.15)

# --- 配置 UGF 编译选项 ---
set(UGF_USE_COCOS_LOG OFF CACHE BOOL "Use cocos2d CCLOG for UGF logging")
set(UGF_BUILD_EXAMPLES OFF CACHE BOOL "Skip UGF examples")

# --- 委托给 UGF 自身的 CMakeLists.txt ---
add_subdirectory(universal_game_framework ${CMAKE_BINARY_DIR}/ugf)
