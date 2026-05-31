// EditorApp.cpp - 动画编辑器主场景（UGF 框架）
#include "EditorApp.h"
#include "EditorEvents.h"
#include "plugins/scene_tree/SceneTreePlugin.h"
#include "plugins/property_editor/PropertyEditorPlugin.h"
#include "plugins/timeline/TimelinePlugin.h"
#include "plugins/behavior_tree/BehaviorTreePlugin.h"
#include "plugins/scene_view/SceneViewPlugin.h"
#include "plugins/file_system/FileSystemPlugin.h"
#include "imgui.h"
#include "imgui_internal.h"
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

    // Start maximized for a proper editor experience
    if (glfwWindow_) {
        glfwMaximizeWindow(glfwWindow_);
        CCLOG("[EditorApp] Window maximized");
    }

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

// ============================================================
//  占位插件 — 演示多组件架构
// ============================================================

class DemoModel : public ugf::Component {
    std::string id_;
public:
    explicit DemoModel(std::string id) : id_(std::move(id)) {}
    std::string getComponentId() const override { return id_ + ".Model"; }
    bool initialize(const std::unordered_map<std::string, std::any>&) override { return true; }
    void update(float) override {}
    void terminate() override {}
    const std::string& getId() const { return id_; }
};

class DemoView : public ugf::Component {
    std::string id_; bool show_ = true;
public:
    explicit DemoView(std::string id) : id_(std::move(id)) {}
    std::string getComponentId() const override { return id_ + ".View"; }
    bool initialize(const std::unordered_map<std::string, std::any>&) override { return true; }
    void update(float) override {
        if (show_) { ImGui::Begin(id_.c_str(), &show_); ImGui::Text("View component"); ImGui::End(); }
    }
    void terminate() override {}
};

class PlaceholderPlugin : public ugf::IPlugin {
    std::string id_;
public:
    explicit PlaceholderPlugin(std::string id) : id_(std::move(id)) {}
    std::string getId() const override { return id_; }
    bool initialize() override {
        componentSystem.registerComponent<DemoModel>("model", {}, id_);
        componentSystem.registerComponent<DemoView>("view", {}, id_);
        return true;
    }
    void update(float dt) override { componentSystem.updateAll(dt); }
};

void EditorApp::registerBuiltinPlugins() {
    pluginSystem_.registerPlugin(std::make_unique<SceneTreePlugin>());
    pluginSystem_.registerPlugin(std::make_unique<PropertyEditorPlugin>());
    pluginSystem_.registerPlugin(std::make_unique<TimelinePlugin>());
    pluginSystem_.registerPlugin(std::make_unique<BehaviorTreePlugin>());
    pluginSystem_.registerPlugin(std::make_unique<SceneViewPlugin>());
    pluginSystem_.registerPlugin(std::make_unique<FileSystemPlugin>());
}

// ============================================================
//  Dockspace — 主编辑器布局
// ============================================================

void EditorApp::setupDockspace() {
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);

    ImGuiWindowFlags dockspaceFlags =
        ImGuiWindowFlags_NoDocking |
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoNavFocus |
        ImGuiWindowFlags_NoBackground;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin("##EditorDockspace", nullptr, dockspaceFlags);
    ImGui::PopStyleVar(3);

    ImGuiID dockspaceId = ImGui::GetID("EditorDockspace");
    ImGui::DockSpace(dockspaceId);

    if (!dockspaceSetup_) {
        dockspaceSetup_ = true;
        setupInitialDockLayout(dockspaceId);
    }

    ImGui::End();
}

void EditorApp::setupInitialDockLayout(unsigned int dockspaceId) {
    ImGui::DockBuilderRemoveNode(dockspaceId);
    ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_None);
    ImGui::DockBuilderSetNodeSize(dockspaceId, ImGui::GetMainViewport()->WorkSize);

    // Layout: Left(Scene Tree,16%) | Center(Scene View 65%/BT+Timeline 35%) | Right(Property,20%)
    unsigned int leftId, rightId;
    ImGui::DockBuilderSplitNode(dockspaceId, ImGuiDir_Left, 0.16f, &leftId, &rightId);

    unsigned int farRightId, centerId;
    ImGui::DockBuilderSplitNode(rightId, ImGuiDir_Right, 0.20f, &farRightId, &centerId);

    // Center: top(Scene View) | bottom(BT | Timeline)
    unsigned int centerBottomId, centerTopId;
    ImGui::DockBuilderSplitNode(centerId, ImGuiDir_Down, 0.35f, &centerBottomId, &centerTopId);

    unsigned int btId, tlId;
    ImGui::DockBuilderSplitNode(centerBottomId, ImGuiDir_Right, 0.50f, &tlId, &btId);

    // Split left panel: Scene Tree (top) | File System (bottom)
    unsigned int leftTopId, leftBottomId;
    ImGui::DockBuilderSplitNode(leftId, ImGuiDir_Down, 0.50f, &leftBottomId, &leftTopId);

    ImGui::DockBuilderDockWindow("Scene Tree", leftTopId);
    ImGui::DockBuilderDockWindow("File System", leftBottomId);
    ImGui::DockBuilderDockWindow("Scene View", centerTopId);
    ImGui::DockBuilderDockWindow("Behavior Tree", btId);
    ImGui::DockBuilderDockWindow("Timeline", tlId);
    ImGui::DockBuilderDockWindow("Property Editor", farRightId);

    ImGui::DockBuilderFinish(dockspaceId);
}

// ImGui NewFrame/Render in visit() — cocos2d-x calls visit() before update()
void EditorApp::visit(Renderer* r, const Mat4& t, uint32_t f) {
    Scene::visit(r, t, f);
    if (!imguiBackendsReady_) return;

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    setupDockspace();
    pluginSystem_.updateAll(0.016f);
    ugf::EventBus::getInstance().update();
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}
