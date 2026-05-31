// SceneTreePlugin.cpp - 场景层级树插件实现（多组件架构）
// 支持右键菜单、节点重命名、复制、拖拽排序/重设父节点
#include "plugins/scene_tree/SceneTreePlugin.h"
#include "editor/EditorEvents.h"
#include "EventSystem.hpp"
#include "imgui.h"
#include "tests/TestFramework.h"
#include "cocos2d.h"
#include <algorithm>
#include <cstring>

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

int SceneTreeModel::countSubtreeNodes(const TreeNode* node) const {
    int count = 1; // this node
    for (const auto* child : node->children) {
        count += countSubtreeNodes(child);
    }
    return count;
}

bool SceneTreeModel::removeNodeRecursive(TreeNode* parent,
                                          const std::string& name) {
    for (auto it = parent->children.begin(); it != parent->children.end(); ++it) {
        if ((*it)->name == name) {
            if (selectedNode_ == *it) selectedNode_ = nullptr;
            int subtreeCount = countSubtreeNodes(*it);
            delete *it; // ~TreeNode recursively deletes all children
            parent->children.erase(it);
            nodeCount_ -= subtreeCount;
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

// ---- Rename / Duplicate / Move ----

bool SceneTreeModel::renameNode(const std::string& oldName,
                                 const std::string& newName) {
    if (newName.empty()) return false;
    if (oldName == "SceneRoot") return false;
    if (oldName == newName) return true;
    if (findNode(newName)) return false; // name collision
    TreeNode* node = findNode(oldName);
    if (!node) return false;
    node->name = newName;
    return true;
}

bool SceneTreeModel::duplicateNode(const std::string& name) {
    TreeNode* source = findNode(name);
    if (!source || !source->parent) return false;

    // Generate unique name: "Name (Copy)", "Name (Copy 2)", ...
    std::string copyName = source->name + " (Copy)";
    int suffix = 1;
    while (findNode(copyName)) {
        ++suffix;
        copyName = source->name + " (Copy " + std::to_string(suffix) + ")";
    }

    TreeNode* copy = new TreeNode(copyName);
    deepCopyChildren(copy, source);

    // Insert right after source in parent's child list
    auto& siblings = source->parent->children;
    auto it = std::find(siblings.begin(), siblings.end(), source);
    siblings.insert(it + 1, copy);
    copy->parent = source->parent;
    ++nodeCount_;
    return true;
}

void SceneTreeModel::deepCopyChildren(TreeNode* dest, TreeNode* src) {
    for (auto* child : src->children) {
        TreeNode* copy = new TreeNode(child->name);
        dest->children.push_back(copy);
        copy->parent = dest;
        ++nodeCount_;
        deepCopyChildren(copy, child);
    }
}

bool SceneTreeModel::moveNode(const std::string& name,
                               const std::string& targetName,
                               InsertPosition pos) {
    TreeNode* source = findNode(name);
    TreeNode* target = findNode(targetName);
    if (!source || !target) return false;
    if (source == target) return false;               // self-drop
    if (source->parent == nullptr) return false;      // cannot move root
    if (isDescendantOf(target, source)) return false; // circular dependency

    // Detach from current parent (no delete, no nodeCount change)
    auto& oldSiblings = source->parent->children;
    oldSiblings.erase(
        std::remove(oldSiblings.begin(), oldSiblings.end(), source),
        oldSiblings.end());

    // Attach to new position
    if (pos == InsertPosition::AsChild) {
        target->children.push_back(source);
        source->parent = target;
    } else {
        TreeNode* newParent = target->parent;
        if (!newParent) {
            // Target is root, fall back to child insertion
            target->children.push_back(source);
            source->parent = target;
        } else {
            auto& newSiblings = newParent->children;
            auto it = std::find(newSiblings.begin(), newSiblings.end(), target);
            if (pos == InsertPosition::AfterSibling) {
                newSiblings.insert(it + 1, source);
            } else {
                newSiblings.insert(it, source);
            }
            source->parent = newParent;
        }
    }
    return true;
}

bool SceneTreeModel::isDescendantOf(TreeNode* node,
                                     TreeNode* potentialAncestor) const {
    if (!node || !potentialAncestor) return false;
    TreeNode* current = node->parent;
    while (current) {
        if (current == potentialAncestor) return true;
        current = current->parent;
    }
    return false;
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

    // Clear drag-drop tracking when no drag is active
    if (ImGui::GetDragDropPayload() == nullptr) {
        draggedNodeName_.clear();
        dropTargetNodeName_.clear();
    }

    ImGui::SetNextWindowSize(ImVec2(300, 400), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Scene Tree", &windowOpen_)) {
        ImGui::End();
        return;
    }

    if (model_ && model_->getRoot()) {
        renderNode(model_->getRoot());
    }

    // Popup modals (rendered inside the window for ID stack consistency)
    renderPendingPopups();

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

    // Inline rename mode: replace TreeNode with an InputText
    if (renaming_ && renameTarget_ == node->name) {
        renderRenameInput(node);
        return;
    }

    // Dim node while it is being dragged
    bool isDragged = (!draggedNodeName_.empty()
                      && draggedNodeName_ == node->name);
    if (isDragged) {
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.4f);
    }

    // Build TreeNode flags
    bool isTarget = (!dropTargetNodeName_.empty()
                     && dropTargetNodeName_ == node->name);
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow;
    if (node->children.empty()) flags |= ImGuiTreeNodeFlags_Leaf;
    if (node->selected) flags |= ImGuiTreeNodeFlags_Selected;
    if (node->parent == nullptr) flags |= ImGuiTreeNodeFlags_DefaultOpen;
    if (isTarget) flags |= ImGuiTreeNodeFlags_Framed;

    bool opened = ImGui::TreeNodeEx(node->name.c_str(), flags);

    // Left-click => select
    if (ImGui::IsItemClicked() && controller_) {
        controller_->onNodeClicked(node->name);
    }

    // Right-click context menu
    renderContextMenu(node);

    // Drag source (non-root nodes only)
    if (node->parent != nullptr) {
        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
            ImGui::SetDragDropPayload("SCENE_NODE", node->name.c_str(),
                                       node->name.size() + 1);
            ImGui::Text("Move: %s", node->name.c_str());
            ImGui::EndDragDropSource();
            draggedNodeName_ = node->name;
        }
    }

    // Drop target
    if (ImGui::BeginDragDropTarget()) {
        const ImGuiPayload* payload =
            ImGui::AcceptDragDropPayload("SCENE_NODE");
        if (ImGui::IsItemHovered(
                ImGuiHoveredFlags_AllowWhenBlockedByPopup) || payload) {
            dropTargetNodeName_ = node->name;
        }
        if (payload) {
            std::string draggedName(
                static_cast<const char*>(payload->Data));
            if (draggedName != node->name) {
                InsertPosition pos = InsertPosition::AsChild;
                if (ImGui::GetIO().KeyShift) {
                    pos = InsertPosition::BeforeSibling;
                } else if (ImGui::GetIO().KeyCtrl) {
                    pos = InsertPosition::AfterSibling;
                }
                model_->moveNode(draggedName, node->name, pos);
            }
            draggedNodeName_.clear();
            dropTargetNodeName_.clear();
        }
        ImGui::EndDragDropTarget();
    }

    // Restore alpha
    if (isDragged) {
        ImGui::PopStyleVar();
    }

    // Recurse into children
    if (opened) {
        for (auto* child : node->children) {
            renderNode(child);
        }
        ImGui::TreePop();
    }
}

void SceneTreeView::renderContextMenu(TreeNode* node) {
    if (!ImGui::BeginPopupContextItem()) return;

    if (ImGui::MenuItem("Add Child Node")) {
        pendingAddChildParent_ = node->name;
        addChildBuffer_[0] = '\0';
    }
    if (ImGui::MenuItem("Rename")) {
        startRename(node);
    }
    if (ImGui::MenuItem("Duplicate")) {
        model_->duplicateNode(node->name);
    }

    ImGui::Separator();

    if (node->name != "SceneRoot") {
        if (ImGui::MenuItem("Delete")) {
            pendingDeleteTarget_ = node->name;
        }
    }

    ImGui::EndPopup();
}

void SceneTreeView::renderRenameInput(TreeNode* node) {
    ImGui::SetNextItemWidth(200);
    ImGuiInputTextFlags f = ImGuiInputTextFlags_EnterReturnsTrue
                            | ImGuiInputTextFlags_AutoSelectAll;
    if (ImGui::InputText("##rename", renameBuffer_,
                          sizeof(renameBuffer_), f)) {
        finishRename();
    }
    // Commit on focus loss (click away)
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        finishRename();
    }
}

void SceneTreeView::startRename(TreeNode* node) {
    renaming_ = true;
    renameTarget_ = node->name;
    std::strncpy(renameBuffer_, node->name.c_str(), sizeof(renameBuffer_) - 1);
    renameBuffer_[sizeof(renameBuffer_) - 1] = '\0';
}

void SceneTreeView::finishRename() {
    if (!renaming_) return;
    if (std::strlen(renameBuffer_) > 0 && renameTarget_ != renameBuffer_) {
        model_->renameNode(renameTarget_, renameBuffer_);
    }
    renaming_ = false;
    renameTarget_.clear();
}

void SceneTreeView::renderPendingPopups() {
    if (!model_) return;

    // --- Delete confirmation modal ---
    if (!pendingDeleteTarget_.empty()) {
        ImGui::OpenPopup("DeleteConfirm");
    }
    if (ImGui::BeginPopupModal("DeleteConfirm", nullptr,
                                ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Delete '%s' and all its children?",
                    pendingDeleteTarget_.c_str());
        if (ImGui::Button("Yes", ImVec2(80, 0))) {
            model_->removeNode(pendingDeleteTarget_);
            pendingDeleteTarget_.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("No", ImVec2(80, 0))) {
            pendingDeleteTarget_.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // --- Add Child popup ---
    if (!pendingAddChildParent_.empty()) {
        ImGui::OpenPopup("AddChildPopup");
    }
    if (ImGui::BeginPopupModal("AddChildPopup", nullptr,
                                ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("New child of '%s':", pendingAddChildParent_.c_str());
        ImGui::SetNextItemWidth(200);
        bool enterPressed = ImGui::InputText(
            "##addchildname", addChildBuffer_,
            sizeof(addChildBuffer_),
            ImGuiInputTextFlags_EnterReturnsTrue
            | ImGuiInputTextFlags_AutoSelectAll);
        bool ok = ImGui::Button("OK", ImVec2(80, 0));
        ImGui::SameLine();
        bool cancel = ImGui::Button("Cancel", ImVec2(80, 0));
        if (ok || enterPressed) {
            if (std::strlen(addChildBuffer_) > 0) {
                TreeNode* parent =
                    model_->findNode(pendingAddChildParent_);
                model_->addNode(addChildBuffer_, parent);
            }
            pendingAddChildParent_.clear();
            ImGui::CloseCurrentPopup();
        }
        if (cancel) {
            pendingAddChildParent_.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
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

    // 2) 注册 Controller
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
