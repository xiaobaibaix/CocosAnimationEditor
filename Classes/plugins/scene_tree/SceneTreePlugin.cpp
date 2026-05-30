// SceneTreePlugin.cpp - 场景层级树插件实现（多组件架构）
#include "plugins/scene_tree/SceneTreePlugin.h"
#include "editor/EditorEvents.h"
#include "EventSystem.hpp"
#include "imgui.h"
#include "tests/TestFramework.h"
#include "cocos2d.h"

// ============================================================
//  TreeNode
// ============================================================

TreeNode::~TreeNode() {
    for (auto* child : children) {
        delete child;
    }
}

// ============================================================
//  SceneTreeModel : ugf::Component
// ============================================================

SceneTreeModel::SceneTreeModel() {
    root_ = new TreeNode("SceneRoot");
    addNode("Bone_Arm", root_);
    addNode("Bone_Leg", root_);
    addNode("Sprite_Hero", root_);
    CCLOG("[SceneTreeModel] Created with %d nodes", nodeCount_);
}

SceneTreeModel::~SceneTreeModel() {
    delete root_;
}

std::string SceneTreeModel::getComponentId() const {
    return "SceneTree.Model";
}

bool SceneTreeModel::initialize(const std::unordered_map<std::string, std::any>&) {
    return true;
}

void SceneTreeModel::update(float) {
    // 数据层无每帧逻辑
}

void SceneTreeModel::terminate() {
    delete root_;
    root_ = nullptr;
    selectedNode_ = nullptr;
    nodeCount_ = 0;
}

TreeNode* SceneTreeModel::addNode(const std::string& name, TreeNode* parent) {
    if (!parent) parent = root_;
    auto* node = new TreeNode(name);
    node->parent = parent;
    parent->children.push_back(node);
    ++nodeCount_;
    return node;
}

bool SceneTreeModel::removeNode(const std::string& name) {
    if (name == "SceneRoot") return false;
    return removeNodeRecursive(root_, name);
}

bool SceneTreeModel::removeNodeRecursive(TreeNode* parent,
                                          const std::string& name) {
    for (auto it = parent->children.begin(); it != parent->children.end(); ++it) {
        if ((*it)->name == name) {
            if (selectedNode_ == *it) selectedNode_ = nullptr;
            delete *it;
            parent->children.erase(it);
            --nodeCount_;
            return true;
        }
        if (removeNodeRecursive(*it, name)) return true;
    }
    return false;
}

void SceneTreeModel::selectNode(const std::string& name) {
    TreeNode* node = findNode(name);
    if (!node) return;
    clearSelection(root_);
    node->selected = true;
    selectedNode_ = node;
}

void SceneTreeModel::clearSelection(TreeNode* node) {
    node->selected = false;
    for (auto* child : node->children) {
        clearSelection(child);
    }
}

TreeNode* SceneTreeModel::findNode(const std::string& name) const {
    return findNodeRecursive(root_, name);
}

TreeNode* SceneTreeModel::findNodeRecursive(TreeNode* node,
                                             const std::string& name) const {
    if (node->name == name) return node;
    for (auto* child : node->children) {
        TreeNode* found = findNodeRecursive(child, name);
        if (found) return found;
    }
    return nullptr;
}

// ============================================================
//  SceneTreeController : ugf::Component
// ============================================================

std::string SceneTreeController::getComponentId() const {
    return "SceneTree.Controller";
}

bool SceneTreeController::initialize(
    const std::unordered_map<std::string, std::any>& config) {
    auto it = config.find("model");
    if (it != config.end()) {
        model_ = std::any_cast<SceneTreeModel*>(it->second);
    }
    CCLOG("[SceneTreeController] Initialized (model=%p)", static_cast<void*>(model_));
    return true;
}

void SceneTreeController::update(float) {
    // 逻辑层无每帧逻辑
}

void SceneTreeController::terminate() {
    model_ = nullptr;
}

void SceneTreeController::onNodeClicked(const std::string& name) {
    if (!model_) return;
    model_->selectNode(name);
    CCLOG("[SceneTree] Selected: %s", name.c_str());
    ugf::EventBus::getInstance().publish(NodeSelectedEvent{name});
}

// ============================================================
//  SceneTreeView : ugf::Component
// ============================================================

std::string SceneTreeView::getComponentId() const {
    return "SceneTree.View";
}

bool SceneTreeView::initialize(
    const std::unordered_map<std::string, std::any>& config) {
    auto itModel = config.find("model");
    if (itModel != config.end()) {
        model_ = std::any_cast<SceneTreeModel*>(itModel->second);
    }
    auto itCtrl = config.find("controller");
    if (itCtrl != config.end()) {
        controller_ = std::any_cast<SceneTreeController*>(itCtrl->second);
    }
    CCLOG("[SceneTreeView] Initialized (model=%p, controller=%p)",
          static_cast<void*>(model_), static_cast<void*>(controller_));
    return true;
}

void SceneTreeView::update(float) {
    if (!windowOpen_) return;

    ImGui::SetNextWindowSize(ImVec2(300, 400), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Scene Tree", &windowOpen_)) {
        ImGui::End();
        return;
    }

    if (model_ && model_->getRoot()) {
        renderNode(model_->getRoot());
    }

    ImGui::End();

    // GUI 测试运行器
    TestFramework::getInstance().runGuiTests("SceneTree");
}

void SceneTreeView::terminate() {
    model_ = nullptr;
    controller_ = nullptr;
}

void SceneTreeView::renderNode(TreeNode* node) {
    if (!node) return;

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow;
    if (node->children.empty()) {
        flags |= ImGuiTreeNodeFlags_Leaf;
    }
    if (node->selected) {
        flags |= ImGuiTreeNodeFlags_Selected;
    }
    if (node->parent == nullptr) {
        flags |= ImGuiTreeNodeFlags_DefaultOpen;
    }

    bool opened = ImGui::TreeNodeEx(node->name.c_str(), flags);

    if (ImGui::IsItemClicked() && controller_) {
        controller_->onNodeClicked(node->name);
    }

    if (opened) {
        for (auto* child : node->children) {
            renderNode(child);
        }
        ImGui::TreePop();
    }
}

// ============================================================
//  API Tests
// ============================================================

static bool test_add_node() {
    SceneTreeModel model;
    if (model.getNodeCount() != 4) return false;
    model.addNode("TestNode", model.getRoot());
    if (model.getNodeCount() != 5) return false;
    return model.findNode("TestNode") != nullptr;
}
REGISTER_API_TEST("SceneTree", test_add_node);

static bool test_remove_node() {
    SceneTreeModel model;
    model.addNode("ToRemove", model.getRoot());
    if (model.getNodeCount() != 5) return false;
    if (!model.removeNode("ToRemove")) return false;
    if (model.getNodeCount() != 4) return false;
    return model.findNode("ToRemove") == nullptr;
}
REGISTER_API_TEST("SceneTree", test_remove_node);

static bool test_get_selected_node() {
    SceneTreeModel model;
    if (model.getSelectedNode() != nullptr) return false;
    model.selectNode("Bone_Arm");
    TreeNode* sel = model.getSelectedNode();
    if (!sel || sel->name != "Bone_Arm") return false;
    return sel->selected;
}
REGISTER_API_TEST("SceneTree", test_get_selected_node);

static bool test_select_clears_previous() {
    SceneTreeModel model;
    model.selectNode("Bone_Arm");
    model.selectNode("Bone_Leg");
    TreeNode* sel = model.getSelectedNode();
    if (!sel || sel->name != "Bone_Leg") return false;
    TreeNode* arm = model.findNode("Bone_Arm");
    return arm && !arm->selected;
}
REGISTER_API_TEST("SceneTree", test_select_clears_previous);

static bool test_cannot_remove_root() {
    SceneTreeModel model;
    if (model.removeNode("SceneRoot")) return false;
    if (model.getNodeCount() != 4) return false;
    return model.findNode("SceneRoot") != nullptr;
}
REGISTER_API_TEST("SceneTree", test_cannot_remove_root);

// ============================================================
//  GUI Tests
// ============================================================

static void test_render_tree() {
    ImGui::Text("Scene tree is rendered in the plugin's main window.");
    ImGui::Text("Look for the 'Scene Tree' window with:");
    ImGui::BulletText("SceneRoot");
    ImGui::BulletText("  Bone_Arm");
    ImGui::BulletText("  Bone_Leg");
    ImGui::BulletText("  Sprite_Hero");
}
REGISTER_GUI_TEST("SceneTree", test_render_tree);

// ============================================================
//  SceneTreePlugin : ugf::IPlugin
// ============================================================

std::string SceneTreePlugin::getId() const {
    return "SceneTree";
}

bool SceneTreePlugin::initialize() {
    CCLOG("[SceneTreePlugin] Initializing...");

    // 1) 注册 Model
    auto* model = componentSystem.registerComponent<SceneTreeModel>("model");
    if (!model) {
        CCLOG("[SceneTreePlugin] Failed to register SceneTreeModel");
        return false;
    }

    // 2) 注册 View，传入 Model 和 Controller（Controller 在后面注册）
    // 先注册 View，Controller 注册后 View 侧指针需要通过某种方式填充
    // 修正：先注册 Model 和 Controller，再注册 View
    std::unordered_map<std::string, std::any> ctrlCfg;
    ctrlCfg["model"] = model;
    auto* controller = componentSystem.registerComponent<SceneTreeController>(
        "controller", ctrlCfg);
    if (!controller) {
        CCLOG("[SceneTreePlugin] Failed to register SceneTreeController");
        return false;
    }

    // 3) 注册 View，传入 Model 和 Controller
    std::unordered_map<std::string, std::any> viewCfg;
    viewCfg["model"] = model;
    viewCfg["controller"] = controller;
    auto* view = componentSystem.registerComponent<SceneTreeView>(
        "view", viewCfg);
    if (!view) {
        CCLOG("[SceneTreePlugin] Failed to register SceneTreeView");
        return false;
    }

    // 运行 API 测试
    TestFramework::getInstance().runApiTests("SceneTree");

    CCLOG("[SceneTreePlugin] Initialized — %zu components, %d tree nodes",
          componentSystem.size(), model->getNodeCount());
    return true;
}

void SceneTreePlugin::update(float deltaTime) {
    componentSystem.updateAll(deltaTime);
}

void SceneTreePlugin::shutdown() {
    CCLOG("[SceneTreePlugin] Shutting down.");
    componentSystem.clear();
}
