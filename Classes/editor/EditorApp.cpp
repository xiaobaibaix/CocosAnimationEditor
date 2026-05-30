// EditorApp.cpp - 动画编辑器主场景实现
// ImGui 渲染后端：GLFW + OpenGL3
#include "EditorApp.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

// cocos2d desktop GLFW 头文件
#include "platform/desktop/CCGLViewImpl-desktop.h"

USING_NS_CC;

// ============================================================
//  Scene factory
// ============================================================

Scene* EditorApp::createScene() {
    return EditorApp::create();
}

// ============================================================
//  Lifecycle
// ============================================================

bool EditorApp::init() {
    if (!Scene::init()) return false;
    return true;
}

void EditorApp::onEnter() {
    Scene::onEnter();

    // 1. 获取 cocos2d 的 GLFWwindow
    auto* director = Director::getInstance();
    auto* glView = dynamic_cast<GLViewImpl*>(director->getOpenGLView());
    if (!glView) {
        CCLOG("[EditorApp] ERROR: cannot get GLViewImpl");
        return;
    }
    glfwWindow_ = glView->getWindow();
    if (!glfwWindow_) {
        CCLOG("[EditorApp] ERROR: GLFWwindow is null");
        return;
    }

    // 2. 设置 ImGui 后端
    setupImGuiBackends();

    // 3. 注册内置插件
    registerBuiltinPlugins();

    // 4. 初始化所有插件
    PluginManager::getInstance().initAll();

    // 5. 注册每帧更新
    schedule(schedule_selector(EditorApp::onEditorUpdate), 0.0f);

    CCLOG("[EditorApp] ready, %zu plugins loaded",
          PluginManager::getInstance().getPluginCount());
}

void EditorApp::onExit() {
    PluginManager::getInstance().shutdownAll();
    shutdownImGuiBackends();
    Scene::onExit();
}

// ============================================================
//  ImGui Backend Setup
// ============================================================

void EditorApp::setupImGuiBackends() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    ImGui::StyleColorsDark();

    if (!ImGui_ImplGlfw_InitForOpenGL(glfwWindow_, true)) {
        CCLOG("[EditorApp] ERROR: ImGui_ImplGlfw_InitForOpenGL failed");
        return;
    }

    const char* glslVersion = "#version 130";
    if (!ImGui_ImplOpenGL3_Init(glslVersion)) {
        CCLOG("[EditorApp] ERROR: ImGui_ImplOpenGL3_Init failed");
        return;
    }

    imguiBackendsReady_ = true;
    CCLOG("[EditorApp] ImGui backends ready (GLFW + OpenGL3)");
}

void EditorApp::shutdownImGuiBackends() {
    if (imguiBackendsReady_) {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        imguiBackendsReady_ = false;
    }
    ImGui::DestroyContext();
}

// ============================================================
//  Plugin Registration
// ============================================================

class PlaceholderPlugin : public IEditorPlugin {
public:
    PlaceholderPlugin(const char* name) : name_(name) {}
    const char* getName() const override { return name_.c_str(); }
    bool onInit() override { return true; }
    void onUpdate(float /*dt*/) override {
        if (showDemo_) {
            ImGui::Begin(name_.c_str(), &showDemo_);
            ImGui::Text("Placeholder - waiting for implementation");
            ImGui::End();
        }
    }
    void onShutdown() override {}
private:
    std::string name_;
    bool showDemo_ = true;
};

void EditorApp::registerBuiltinPlugins() {
    auto& pm = PluginManager::getInstance();
    pm.registerPlugin(std::make_unique<PlaceholderPlugin>("SceneTree"));
    pm.registerPlugin(std::make_unique<PlaceholderPlugin>("PropertyEditor"));
    pm.registerPlugin(std::make_unique<PlaceholderPlugin>("Timeline"));
    pm.registerPlugin(std::make_unique<PlaceholderPlugin>("BehaviorTree"));
}

// ============================================================
//  Main Loop
// ============================================================

void EditorApp::onEditorUpdate(float /*dt*/) {
    if (!imguiBackendsReady_) return;

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    PluginManager::getInstance().updateAll(0.016f);
}

void EditorApp::visit(Renderer* renderer, const Mat4& transform, uint32_t parentFlags) {
    Scene::visit(renderer, transform, parentFlags);

    if (imguiBackendsReady_) {
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }
}
