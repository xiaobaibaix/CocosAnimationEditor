# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Cocos2d-x C++ project using engine version **cocos2d-x-3.17.2** (MIT license). The engine source is bundled at `Library/cocos2d/`. Currently a template/boilerplate project intended to become an animation editor.

## Build & Run

### Linux (primary development platform)

```bash
# Full build
./build.sh

# Manual CMake configuration and build
cmake -S . -B proj.linux/build -DCMAKE_BUILD_TYPE=Debug
cmake --build proj.linux/build -- -j$(nproc)

# Run the built executable
./proj.linux/build/bin/CocosAnimationEditor/CocosAnimationEditor
```

The build script (`build.sh`) auto-detects first-run (no Makefile yet) and runs CMake configuration. On subsequent runs it only compiles changed files.

### Other platforms

- **Windows**: Visual Studio solution at `proj.win32/CocosAnimationEditor.sln`
- **Android**: Gradle project at `proj.android/` (run `./gradlew assembleDebug`)
- **iOS/macOS**: Xcode project generated via CMake with `-G Xcode` targeting `proj.ios_mac/`

## Architecture

### Application Lifecycle

```
platform main.cpp → AppDelegate → Director → Scene (HelloWorld)
```

- **`proj.linux/main.cpp`** (and equivalents for other platforms): Entry point. Creates `AppDelegate` instance, calls `Application::getInstance()->run()`.
- **`Classes/AppDelegate`**: Inherits from `cocos2d::Application` (private inheritance). Manages:
  - OpenGL context setup (`initGLContextAttrs`)
  - App launch (`applicationDidFinishLaunching`) — creates the GL view, configures resolution policy, sets design resolution (480×320), and runs the initial scene
  - Background/foreground transitions — pauses/resumes audio and animation
- **`Classes/HelloWorldScene`**: Main scene (currently template code). Inherits from `cocos2d::Scene`. Uses `CREATE_FUNC` macro for cocos2d factory pattern.

### CMake Build Structure

- `CMakeLists.txt` is the single build file covering all platforms
- Platform detection via CMake variables (`ANDROID`, `LINUX`, `WINDOWS`, `APPLE` + `IOS`/`MACOSX`)
- Source files listed per-platform in `GAME_SOURCE` / `GAME_HEADER` lists
- Links against `cocos2d` library built from `Library/cocos2d/cocos/`
- On Linux: links with `-no-pie` (position-independent executable disabled)
- Resources are copied to build output directory at build time

### Key Files

| File | Purpose |
|------|---------|
| `CMakeLists.txt` | Build configuration for all platforms |
| `Classes/AppDelegate.h/.cpp` | Application lifecycle, GL setup, scene bootstrap |
| `Classes/HelloWorldScene.h/.cpp` | Main scene (template — replace with actual editor UI) |
| `proj.linux/main.cpp` | Linux entry point |
| `proj.linux/math-compat.cpp` | Math linking shims (`__powf_finite`, `__expf_finite`) for glibc compatibility |
| `build.sh` | Linux build convenience script |
| `Resources/` | Game assets (PNGs, fonts) — auto-copied to build output |
| `Library/cocos2d/` | Bundled cocos2d-x engine source |

### Design Resolution

The app uses a design resolution of **480×320** with `ResolutionPolicy::NO_BORDER`. Three resolution tiers are defined:
- Small: 480×320
- Medium: 1024×768
- Large: 2048×1536

Content scale factor is auto-selected based on frame size at startup.

### Audio Engine

Audio is disabled by default. To enable, uncomment one of the `#define` lines in `AppDelegate.cpp`:
- `USE_AUDIO_ENGINE` — cocos2d experimental audio engine
- `USE_SIMPLE_AUDIO_ENGINE` — legacy simple audio engine

Only one may be enabled at a time.

## Third-Party Library Integration

### Directory Convention

Each third-party library lives under `Library/<name>/` with this structure:

```
Library/<name>/
├── <name>/              ← git submodule (the actual library source)
└── CMakeLists.txt       ← custom CMake wrapper, NEVER modify submodule files
```

Example (behaviac):
```
Library/behaviac/
├── behaviac/            ← https://github.com/Tencent/behaviac.git
└── CMakeLists.txt       ← compiles libbehaviac (STATIC)
```

### Adding a New Third-Party Library

**Step 1: Add git submodule**
```bash
git submodule add -b <branch> <url> Library/<name>/<name>
```

**Step 2: Update `.gitmodules`** to reflect the nested path:
```
[submodule "Library/<name>/<name>"]
    path = Library/<name>/<name>
    url = <url>
    branch = <branch>
```

**Step 3: Create `Library/<name>/CMakeLists.txt`**

Follow this template:
```cmake
cmake_minimum_required(VERSION 3.6)

# Root of the submodule source
set(<NAME>_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/<name>)

# Collect source files (list explicitly or use file(GLOB))
set(<NAME>_SOURCES
    ${<NAME>_ROOT}/src/foo.cpp
    # ...
)

# Build as static library
add_library(<libname> STATIC ${<NAME>_SOURCES})

# Public include paths
target_include_directories(<libname> PUBLIC ${<NAME>_ROOT}/include)

# Platform-specific compile definitions (if needed)
target_compile_definitions(<libname> PUBLIC <NAME>_STATIC)

# Suppress warnings for third-party code
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(<libname> PRIVATE -Wno-...)
endif()
```

**Key rules:**
- Use `STATIC` library type (simplest integration, no DLL export hassle)
- Set `<LIB>_STATIC` define so the library knows it's being built statically
- Source root points to `${CMAKE_CURRENT_SOURCE_DIR}/<name>` (the submodule sibling)
- Suppress warnings on third-party code with `-Wno-*` flags
- **NEVER** modify files inside the submodule — the CMakeLists.txt wraps around it

**Step 4: Integrate into root `CMakeLists.txt`**

```cmake
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/Library/<name> ${ENGINE_BINARY_PATH}/<name>)
```

Then add to `target_link_libraries`:
```cmake
target_link_libraries(${APP_NAME} ... <libname>)
```

### Current Third-Party Libraries

| Library | Path | Submodule URL | Target Name | Notes |
|---------|------|---------------|-------------|-------|
| Behaviac | `Library/behaviac/behaviac` | https://github.com/Tencent/behaviac.git | `libbehaviac` | Behavior tree AI |
| Dear ImGui | `Library/imgui/imgui` | https://github.com/ocornut/imgui.git (docking) | `imgui_core` | Immediate mode GUI |
| UGF | `Library/ugf/universal_game_framework` | https://gitee.com/huazi427/universal_game_framework.git | `ugf_framework` | Universal Game Framework |

## Branch Strategy (GitFlow)

### Branch Model

```
main       ──●────────────────────────────●──────  ← 生产发布（稳定）
             │                            │
dev        ──┼────●─────●────●─────────┼──────  ← 开发集成
             │     \         /          │
feature/    │      ●──●──●──┘          │
             │                          │
hotfix/     ●──────────────────────────┘        ← 紧急修复
```

| 分支 | 用途 | 从哪切 | 合并到哪 |
|------|------|--------|---------|
| `main` | 生产稳定，始终可编译运行 | — | — |
| `dev` | 日常集成，所有功能先到这里 | `main` | — |
| `feature/<name>` | 单一功能开发 | `dev` | `dev` |
| `fix/<name>` | Bug 修复 | `dev` | `dev` |
| `release/<ver>` | 发布准备 | `dev` | `main` + `dev` |
| `hotfix/<name>` | 紧急线上修复 | `main` | `main` + `dev` |

**命名约定**：分支名用小写 + 连字符，如 `feature/imgui-opengl-backend`、`fix/crash-on-exit`

### Daily Workflow

```bash
# 1. 开始新功能
git checkout dev
git pull origin dev
git checkout -b feature/my-feature

# 2. 开发 & 提交（遵循 conventional commits）
git add -A
git commit -m "feat: add my feature"

# 3. 推送并创建 PR（dev ← feature/my-feature）
git push -u origin feature/my-feature
gh pr create --base dev --head feature/my-feature --title "feat: my feature"

# 4. PR 合并后清理
git checkout dev
git pull origin dev
git branch -d feature/my-feature
```

### Commit Message Format

```
<type>: <description>

type: feat | fix | refactor | docs | test | chore | perf | ci
```

### Pre-Commit Checklist

- [ ] 编译通过：`cmake --build proj.linux/build -- -j$(nproc)`
- [ ] 子模块初始化：`git submodule update --init --recursive`
- [ ] 不修改子模块内部文件 (`Library/*/`)
- [ ] Commit message 符合 conventional commits 格式
- [ ] PR 目标分支正确（feature → `dev`，release → `main`）

> 完整的开发流程（研究 → 计划 → TDD → 代码审查）参见 `~/.claude/rules/common/development-workflow.md`

## Agent Team (代理团队)

本项目配置了 5 个专用 AI 代理 + 1 个工作流脚本，用于并行开发 4 个 feature 插件。

### 代理架构

```
                    ┌──────────────────────┐
                    │   Integrator (Opus)   │  ← 集成协调、代码审查、合并
                    └──────────┬───────────┘
                               │
       ┌───────────────────────┼───────────────────────┐
       │                       │                       │
       ▼                       ▼                       ▼
┌──────────────┐  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐
│ BehaviorTree │  │PropertyEditor│  │  SceneTree   │  │  Timeline    │
│  (Sonnet)    │  │  (Sonnet)    │  │  (Sonnet)    │  │  (Sonnet)    │
├──────────────┤  ├──────────────┤  ├──────────────┤  ├──────────────┤
│ feature/     │  │ feature/     │  │ feature/     │  │ feature/     │
│ behavior-tree│  │ property-    │  │ scene-tree   │  │ timeline     │
│              │  │ editor       │  │              │  │              │
└──────────────┘  └──────────────┘  └──────────────┘  └──────────────┘
```

### 代理列表

| 代理名称 | 配置文件 | 模型 | 负责分支 | 职责 |
|---------|---------|------|---------|------|
| `feature-behavior-tree` | `.claude/agents/feature-behavior-tree.md` | Sonnet | `feature/behavior-tree` | 行为树插件 (Behaviac + BT 编辑器) |
| `feature-property-editor` | `.claude/agents/feature-property-editor.md` | Sonnet | `feature/property-editor` | 属性编辑器插件 (Inspector + PropertyBag) |
| `feature-scene-tree` | `.claude/agents/feature-scene-tree.md` | Sonnet | `feature/scene-tree` | 场景树插件 (层级管理 + 节点 CRUD) |
| `feature-timeline` | `.claude/agents/feature-timeline.md` | Sonnet | `feature/timeline` | 时间轴插件 (关键帧 + 动画曲线) |
| `integrator` | `.claude/agents/integrator.md` | Opus | `dev` | 集成协调、冲突解决、全量构建验证 |

### 使用方式

#### 1. 单代理模式 — 指派特定 feature 代理

```
# 直接调用某个 feature 代理
使用 feature-behavior-tree 代理，帮我添加行为树节点的复制粘贴功能
```

#### 2. 工作流模式 — 并行开发所有 feature

```
# 启动 feature-team 工作流，传入任务描述
/feature-team 为所有插件添加撤销/重做功能
```

工作流分 3 个阶段：
1. **Setup** — 检查/创建各 feature 分支的 git worktree
2. **Develop** — 4 个 feature 代理并行在独立 worktree 中开发
3. **Integrate** — 集成协调者合并所有分支到 dev 并构建验证

#### 3. 集成模式 — 仅合并和验证

```
# 使用 integrator 代理合并所有 feature 分支到 dev
使用 integrator 代理，将所有 feature 分支合并到 dev 并验证构建
```

### Git Worktree 隔离

每个 feature 代理工作在独立的 git worktree 中（路径：`.claude/worktrees/feature-<name>/`），
互不干扰，无需切换分支。

```bash
# 手动创建所有 feature worktree（可选，工作流会自动创建）
for branch in feature/behavior-tree feature/property-editor feature/scene-tree feature/timeline; do
  name=$(basename $branch)
  git worktree add .claude/worktrees/$name $branch 2>/dev/null || echo "$name already exists"
done

# 查看所有 worktree
git worktree list

# 清理 worktree（完成后）
git worktree remove .claude/worktrees/feature-behavior-tree
```
