// EditorApp.cpp - 动画编辑器主场景（UGF 框架）
#include "EditorApp.h"
#include "EditorEvents.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "platform/desktop/CCGLViewImpl-desktop.h"

USING_NS_CC;

Scene* EditorApp::createScene() { return EditorApp::create(); }
bool EditorApp::init() { return Scene::init(); }

void EditorApp::onEnter() {
    Scene::onEnter();
    auto* glView = dynamic_cast<GLViewImpl*>(Director::getInstance()->getOpenGLView());
    if (!glView) { CCLOG("[EditorApp] no GLViewImpl"); return; }
    glfwWindow_ = glView->getWindow();
    setupImGuiBackends();
    if (!imguiBackendsReady_) return;
    ugf::UGF::getInstance().initialize();
    registerBuiltinPlugins();
    schedule(schedule_selector(EditorApp::onEditorUpdate), 0.0f);
    CCLOG("[EditorApp] %zu plugins loaded", pluginSystem_.size());
}

void EditorApp::onExit() {
    pluginSystem_.clear();
    shutdownImGuiBackends();
    Scene::onExit();
}

void EditorApp::setupImGuiBackends() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    ImGui::StyleColorsDark();
    if (!ImGui_ImplGlfw_InitForOpenGL(glfwWindow_, true) ||
        !ImGui_ImplOpenGL3_Init("#version 130")) {
        CCLOG("[EditorApp] ImGui backend failed"); return;
    }
    imguiBackendsReady_ = true;
}

void EditorApp::shutdownImGuiBackends() {
    if (imguiBackendsReady_) { ImGui_ImplOpenGL3_Shutdown(); ImGui_ImplGlfw_Shutdown(); }
    ImGui::DestroyContext();
}

class PlaceholderPlugin : public ugf::IPlugin {
    std::string id_; bool show_ = true;
public:
    explicit PlaceholderPlugin(std::string id) : id_(std::move(id)) {}
    std::string getId() const override { return id_; }
    bool initialize() override { return true; }
    void update(float) override {
        if (show_) { ImGui::Begin(id_.c_str(), &show_); ImGui::Text("UGF Plugin"); ImGui::End(); }
    }
};

void EditorApp::registerBuiltinPlugins() {
    pluginSystem_.registerPlugin(std::make_unique<PlaceholderPlugin>("SceneTree"));
    pluginSystem_.registerPlugin(std::make_unique<PlaceholderPlugin>("PropertyEditor"));
    pluginSystem_.registerPlugin(std::make_unique<PlaceholderPlugin>("Timeline"));
    pluginSystem_.registerPlugin(std::make_unique<PlaceholderPlugin>("BehaviorTree"));
}

void EditorApp::onEditorUpdate(float) {
    if (!imguiBackendsReady_) return;
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    pluginSystem_.updateAll(0.016f);
    ugf::EventBus::getInstance().update();
}

void EditorApp::visit(Renderer* r, const Mat4& t, uint32_t f) {
    Scene::visit(r, t, f);
    if (imguiBackendsReady_) { ImGui::Render(); ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData()); }
}
