// SceneTreePlugin.cpp - 场景层级树插件实现
#include "plugins/scene_tree/SceneTreePlugin.h"
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
//  SceneTreeModel
// ============================================================

SceneTreeModel::SceneTreeModel() {
    root_ = new TreeNode("SceneRoot");

    // 构建硬编码树结构
    addNode("Bone_Arm", root_);
    addNode("Bone_Leg", root_);
    addNode("Sprite_Hero", root_);
}

SceneTreeModel::~SceneTreeModel() {
    delete root_;
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
            if (selectedNode_ == *it) {
                selectedNode_ = nullptr;
            }
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

    CCLOG("[SceneTree] Selected: %s", name.c_str());
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
//  API Test Registration (static)
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
    bool removed = model.removeNode("ToRemove");
    if (!removed) return false;
    if (model.getNodeCount() != 4) return false;
    return model.findNode("ToRemove") == nullptr;
}
REGISTER_API_TEST("SceneTree", test_remove_node);

static bool test_get_selected_node() {
    SceneTreeModel model;
    if (model.getSelectedNode() != nullptr) return false;
    model.selectNode("Bone_Arm");
    TreeNode* sel = model.getSelectedNode();
    if (sel == nullptr) return false;
    if (sel->name != "Bone_Arm") return false;
    if (!sel->selected) return false;
    return true;
}
REGISTER_API_TEST("SceneTree", test_get_selected_node);

static bool test_select_clears_previous() {
    SceneTreeModel model;
    model.selectNode("Bone_Arm");
    model.selectNode("Bone_Leg");
    TreeNode* sel = model.getSelectedNode();
    if (sel == nullptr || sel->name != "Bone_Leg") return false;
    TreeNode* arm = model.findNode("Bone_Arm");
    if (arm == nullptr || arm->selected) return false;
    return true;
}
REGISTER_API_TEST("SceneTree", test_select_clears_previous);

static bool test_cannot_remove_root() {
    SceneTreeModel model;
    bool removed = model.removeNode("SceneRoot");
    if (removed) return false;
    if (model.getNodeCount() != 4) return false;
    return model.findNode("SceneRoot") != nullptr;
}
REGISTER_API_TEST("SceneTree", test_cannot_remove_root);

// ============================================================
//  GUI Test Registration (static)
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
//  SceneTreePlugin (ugf::IPlugin)
// ============================================================

SceneTreePlugin::SceneTreePlugin() = default;
SceneTreePlugin::~SceneTreePlugin() = default;

std::string SceneTreePlugin::getId() const {
    return "SceneTree";
}

bool SceneTreePlugin::initialize() {
    CCLOG("[SceneTreePlugin] Initializing...");

    TestFramework::getInstance().runApiTests("SceneTree");

    CCLOG("[SceneTreePlugin] Initialized with %d nodes", model_.getNodeCount());
    return true;
}

void SceneTreePlugin::update(float /*deltaTime*/) {
    if (!windowOpen_) return;

    ImGui::SetNextWindowSize(ImVec2(300, 400), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Scene Tree", &windowOpen_)) {
        ImGui::End();
        return;
    }

    renderTreeNode(model_.getRoot());

    ImGui::End();

    TestFramework::getInstance().runGuiTests("SceneTree");
}

void SceneTreePlugin::shutdown() {
    CCLOG("[SceneTreePlugin] Shutting down.");
    componentSystem.clear();
}

void SceneTreePlugin::renderTreeNode(TreeNode* node) {
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

    if (ImGui::IsItemClicked()) {
        model_.selectNode(node->name);
    }

    if (opened) {
        for (auto* child : node->children) {
            renderTreeNode(child);
        }
        ImGui::TreePop();
    }
}
