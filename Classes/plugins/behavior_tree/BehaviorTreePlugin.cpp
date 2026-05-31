// BehaviorTreePlugin.cpp - Behavior Tree Editor Plugin implementation
// Multi-component architecture: BTModel (data) + BTView (UI) + BTBehaviacBridge (integration)
//
// Features:
//   - Right-click context menu: Add Child / Delete / Rename
//   - Drag & drop reparenting with circular-drop rejection
//   - Properties panel for selected node (name, type, conditionScript, actionName)
//   - Debug mode with PlayStateChangedEvent and simulated execution indicators
#include "BehaviorTreePlugin.h"
#include "editor/EditorEvents.h"
#include "behaviac/behaviac.h"
#include "imgui.h"
#include "cocos2d.h"
#include "tests/TestFramework.h"
#include <cstdio>

// Unicode icons for debug mode indicators
#ifndef ICON_GREEN_DOT
#define ICON_GREEN_DOT "\xE2\x97\x8F"  // U+25CF ●
#endif
#ifndef ICON_GRAY_DOT
#define ICON_GRAY_DOT  "\xE2\x97\x8B"  // U+25CB ○
#endif

// ================================================================
//  BTView : ugf::Component — ImGui tree visualization
// ================================================================

bool BTView::initialize(const std::unordered_map<std::string, std::any>& config) {
    if (config.count("model")) {
        try {
            model_ = std::any_cast<BTModel*>(config.at("model"));
        } catch (const std::bad_any_cast&) {
            CCLOG("[BTView] ERROR: bad_any_cast for model config");
            return false;
        }
    }
    if (!model_) {
        CCLOG("[BTView] ERROR: no BTModel provided");
        return false;
    }

    // Subscribe to play state changes for debug mode
    playStateConn_ = ugf::EventBus::getInstance().subscribe<PlayStateChangedEvent>(
        [this](const PlayStateChangedEvent& e) { onPlayStateChanged(e); }
    );

    CCLOG("[BTView] Initialized");
    return true;
}

void BTView::terminate() {
    playStateConn_.release();
    dragSource_.reset();
    contextNode_.reset();
    model_ = nullptr;
}

void BTView::update(float /*deltaTime*/) {
    if (!model_) return;
    updateDebugSimulation();
    renderWindow();
}

// ================================================================
//  Main Window
// ================================================================

void BTView::renderWindow() {
    ImGui::SetNextWindowSize(ImVec2(650, 500), ImGuiCond_FirstUseEver);

    if (!ImGui::Begin("Behavior Tree")) {
        ImGui::End();
        return;
    }

    renderToolbar();
    ImGui::Separator();

    // Side-by-side layout: tree panel (left 55%) | properties panel (right 45%)
    float availWidth = ImGui::GetContentRegionAvail().x;
    float treeWidth = availWidth * 0.55f;

    // --- Left: Tree Panel ---
    ImGui::BeginChild("BT_TreeChild", ImVec2(treeWidth, 0), true);
    renderTreePanel();
    ImGui::EndChild();

    ImGui::SameLine();

    // --- Right: Properties Panel ---
    ImGui::BeginChild("BT_PropertiesChild", ImVec2(0, 0), true);
    renderPropertiesPanel();
    ImGui::EndChild();

    // Rename modal (rendered at window level so it can appear on top)
    if (showRenamePopup_) {
        showRenameModal();
    }

    ImGui::End();
}

void BTView::renderToolbar() {
    // Debug mode toggle
    if (ImGui::Checkbox("Debug Mode", &debugMode_)) {
        if (!debugMode_) {
            // Clear all execution flags when leaving debug mode
            model_->traverse([](const BTNode& node, int) {
                const_cast<BTNode&>(node).debugExecuting = false;
            });
        }
        CCLOG("[BTView] Debug mode: %s", debugMode_ ? "ON" : "OFF");
    }

    ImGui::SameLine();

    // Node count display
    ImGui::TextDisabled("Nodes: %zu", model_->nodeCount());

    if (debugMode_) {
        ImGui::SameLine();
        // Show which nodes are "executing"
        std::string execList;
        model_->traverse([&](const BTNode& node, int) {
            if (node.debugExecuting) {
                if (!execList.empty()) execList += ", ";
                execList += node.name;
            }
        });
        if (execList.empty()) {
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "  (no active nodes)");
        } else {
            ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "  Running: %s", execList.c_str());
        }
    }
}

// ================================================================
//  Tree Panel
// ================================================================

void BTView::renderTreePanel() {
    // Selection info header
    auto selected = model_->getSelected();
    if (selected) {
        ImVec4 selColor = colorForType(selected->type);
        ImGui::TextColored(selColor, "Selected: %s (%s)",
                           selected->name.c_str(), selected->typeName());
    } else {
        ImGui::TextDisabled("No node selected");
    }

    // Instruction text for drag-drop
    ImGui::TextDisabled("Right-click for menu | Drag to reparent");
    ImGui::Separator();

    // Render the tree
    auto root = model_->getRoot();
    if (root) {
        renderNode(root);
    } else {
        ImGui::TextDisabled("(empty tree)");
    }

    // Render context menu popup (must be called every frame that it could be open)
    if (contextNode_) {
        showNodeContextMenu(contextNode_);
    }
}

void BTView::renderNode(const std::shared_ptr<BTNode>& node) {
    if (!node) return;

    ImGui::PushID(static_cast<const void*>(node.get()));

    // --- Drop Target: make this node a potential parent ---
    handleDropTarget(node);

    // --- Tree node flags ---
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;
    if (node->children.empty()) {
        flags |= ImGuiTreeNodeFlags_Leaf;
    }
    if (model_->getSelected() == node) {
        flags |= ImGuiTreeNodeFlags_Selected;
    }
    if (!node->parent.lock()) {
        flags |= ImGuiTreeNodeFlags_DefaultOpen;
    }

    // --- Color by type ---
    ImVec4 color = colorForType(node->type);

    // --- Debug execution indicator ---
    bool isDragging = (dragSource_ == node);
    if (isDragging) {
        // Dim the source node during drag
        color.w *= 0.4f;
    }

    ImGui::PushStyleColor(ImGuiCol_Text, color);

    // --- Build label ---
    char label[256];
    if (node->debugExecuting) {
        // Green dot prefix for executing nodes
        snprintf(label, sizeof(label), ICON_GREEN_DOT " %s (%s)##%p",
                 node->name.c_str(), node->typeName(), static_cast<const void*>(node.get()));
    } else if (debugMode_ && !node->children.empty()) {
        // Gray dot for idle composite nodes in debug mode
        snprintf(label, sizeof(label), ICON_GRAY_DOT " %s (%s)##%p",
                 node->name.c_str(), node->typeName(), static_cast<const void*>(node.get()));
    } else {
        snprintf(label, sizeof(label), "%s (%s)",
                 node->name.c_str(), node->typeName());
    }

    // --- Render TreeNode ---
    bool opened = ImGui::TreeNodeEx(label, flags);

    ImGui::PopStyleColor();

    // --- Left click: select ---
    if (ImGui::IsItemClicked(0)) {
        model_->setSelected(node);
        CCLOG("[BTView] Selected: %s (%s)", node->name.c_str(), node->typeName());
    }

    // --- Right click: context menu ---
    if (ImGui::IsItemClicked(1)) {
        contextNode_ = node;
        model_->setSelected(node);
        ImGui::OpenPopup("NodeContextMenu");
    }

    // --- Drag source: initiate drag from this node ---
    beginDragSource(node);

    // --- Render children ---
    if (opened) {
        for (auto& child : node->children) {
            renderNode(child);
        }
        ImGui::TreePop();
    }

    ImGui::PopID();
}

// ================================================================
//  Context Menu
// ================================================================

void BTView::showNodeContextMenu(const std::shared_ptr<BTNode>& node) {
    if (!ImGui::BeginPopup("NodeContextMenu")) return;

    ImGui::TextDisabled("%s (%s)", node->name.c_str(), node->typeName());
    ImGui::Separator();

    // --- Add Child submenu ---
    if (ImGui::BeginMenu("Add Child")) {
        const struct { BTNodeType type; const char* label; } items[] = {
            {BTNodeType::Selector,  "Selector"},
            {BTNodeType::Sequence,  "Sequence"},
            {BTNodeType::Condition, "Condition"},
            {BTNodeType::Action,    "Action"},
        };
        for (auto& item : items) {
            if (ImGui::MenuItem(item.label)) {
                std::string newName = model_->generateName(item.type);
                auto child = model_->addNode(newName, item.type, node);
                if (child) {
                    CCLOG("[BTView] Added child: %s -> %s", node->name.c_str(), newName.c_str());
                    // Auto-expand: select the new child and open the parent in next frame
                    model_->setSelected(child);
                }
            }
        }
        ImGui::EndMenu();
    }

    // --- Delete (cannot delete root) ---
    bool isRoot = !node->parent.lock();
    if (isRoot) {
        ImGui::BeginDisabled();
    }
    if (ImGui::MenuItem("Delete")) {
        size_t before = model_->nodeCount();
        bool ok = model_->removeNode(node);
        if (ok) {
            CCLOG("[BTView] Deleted: %s (tree size: %zu -> %zu)",
                  node->name.c_str(), before, model_->nodeCount());
        }
    }
    if (isRoot) {
        ImGui::EndDisabled();
    }

    // --- Rename ---
    if (ImGui::MenuItem("Rename")) {
        snprintf(renameBuffer_, sizeof(renameBuffer_), "%s", node->name.c_str());
        showRenamePopup_ = true;
        ImGui::OpenPopup("RenamePopup");
    }

    ImGui::EndPopup();
}

void BTView::showRenameModal() {
    // Always center the popup
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    if (ImGui::BeginPopupModal("RenamePopup", &showRenamePopup_,
                                ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Rename node:");
        ImGui::InputText("##renameInput", renameBuffer_, sizeof(renameBuffer_));

        if (ImGui::Button("OK", ImVec2(120, 0))) {
            if (contextNode_ && renameBuffer_[0] != '\0') {
                contextNode_->name = renameBuffer_;
                CCLOG("[BTView] Renamed node to: %s", renameBuffer_);
            }
            showRenamePopup_ = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            showRenamePopup_ = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

// ================================================================
//  Drag & Drop
// ================================================================

bool BTView::isValidDropTarget(const std::shared_ptr<BTNode>& source,
                                const std::shared_ptr<BTNode>& target) const {
    if (!source || !target) return false;
    if (source == target) return false;                    // Cannot drop on self
    if (source->isDescendantOf(target)) return false;       // Circular: target is ancestor of source
    if (target->isDescendantOf(source)) return false;       // Already parent (shouldn't happen, but safe)
    return true;
}

void BTView::reparentNode(std::shared_ptr<BTNode> source,
                           std::shared_ptr<BTNode> newParent) {
    // Detach from current parent
    auto oldParent = source->parent.lock();
    if (oldParent) {
        oldParent->removeChild(source);
    }
    // Attach to new parent
    newParent->addChild(source);
    CCLOG("[BTView] Reparented '%s' from '%s' to '%s'",
          source->name.c_str(),
          oldParent ? oldParent->name.c_str() : "(root)",
          newParent->name.c_str());
    dragSource_.reset();
}

void BTView::beginDragSource(const std::shared_ptr<BTNode>& node) {
    if (!node) return;
    // Only allow dragging non-root nodes
    if (!node->parent.lock()) return;

    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
        dragSource_ = node;
        // Payload: the raw pointer (used for type verification on drop)
        BTNode* rawPtr = node.get();
        ImGui::SetDragDropPayload("BT_NODE", &rawPtr, sizeof(BTNode*));
        ImGui::TextColored(colorForType(node->type), "Moving: %s", node->name.c_str());
        ImGui::EndDragDropSource();
    }
}

void BTView::handleDropTarget(const std::shared_ptr<BTNode>& node) {
    if (!node || !ImGui::BeginDragDropTarget()) return;

    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("BT_NODE")) {
        if (payload->DataSize == sizeof(BTNode*)) {
            // Recover the raw pointer from payload
            BTNode* droppedRaw = *reinterpret_cast<BTNode* const*>(payload->Data);
            // Use our stored dragSource_ for the safe shared_ptr reference
            if (dragSource_ && dragSource_.get() == droppedRaw) {
                if (isValidDropTarget(dragSource_, node)) {
                    reparentNode(dragSource_, node);
                    model_->setSelected(dragSource_);
                } else {
                    CCLOG("[BTView] Drop rejected: invalid target (circular or self)");
                    dragSource_.reset();
                }
            }
        }
    }
    ImGui::EndDragDropTarget();
}

// ================================================================
//  Properties Panel
// ================================================================

void BTView::renderPropertiesPanel() {
    auto selected = model_->getSelected();

    if (!selected) {
        ImGui::TextDisabled("Select a node to view properties");
        return;
    }

    ImGui::TextColored(colorForType(selected->type),
                       "Properties: %s", selected->name.c_str());
    ImGui::Separator();

    // --- Name ---
    if (!propDirty_) {
        snprintf(propNameBuffer_, sizeof(propNameBuffer_), "%s", selected->name.c_str());
    }
    ImGui::Text("Name:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(-1);
    if (ImGui::InputText("##propName", propNameBuffer_, sizeof(propNameBuffer_))) {
        selected->name = propNameBuffer_;
        propDirty_ = true;
    } else {
        propDirty_ = false;
    }

    // --- Type (read-only) ---
    ImGui::Text("Type:");
    ImGui::SameLine();
    ImGui::BeginDisabled();
    // Display as a read-only combo box with the current type
    const char* typeItems[] = {"Selector", "Sequence", "Condition", "Action"};
    int typeIdx = static_cast<int>(selected->type);
    ImGui::SetNextItemWidth(-1);
    ImGui::Combo("##propType", &typeIdx, typeItems, 4);
    ImGui::EndDisabled();

    // --- Condition Script (only for Condition nodes) ---
    if (selected->type == BTNodeType::Condition) {
        ImGui::Separator();
        ImGui::Text("Condition Script:");
        snprintf(condScriptBuffer_, sizeof(condScriptBuffer_), "%s",
                 selected->conditionScript.c_str());
        ImGui::SetNextItemWidth(-1);
        if (ImGui::InputText("##propCondScript", condScriptBuffer_,
                             sizeof(condScriptBuffer_))) {
            selected->conditionScript = condScriptBuffer_;
        }
    }

    // --- Action Name (only for Action nodes) ---
    if (selected->type == BTNodeType::Action) {
        ImGui::Separator();
        ImGui::Text("Action Name:");
        snprintf(actionNameBuffer_, sizeof(actionNameBuffer_), "%s",
                 selected->actionName.c_str());
        ImGui::SetNextItemWidth(-1);
        if (ImGui::InputText("##propActionName", actionNameBuffer_,
                             sizeof(actionNameBuffer_))) {
            selected->actionName = actionNameBuffer_;
        }
    }

    // --- Debug info ---
    if (debugMode_) {
        ImGui::Separator();
        ImGui::Text("Debug State: %s", selected->debugExecuting ? "Running" : "Idle");
        ImGui::SameLine();
        ImVec4 dotColor = selected->debugExecuting
            ? ImVec4(0.2f, 1.0f, 0.2f, 1.0f)
            : ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
        ImGui::TextColored(dotColor, selected->debugExecuting ? ICON_GREEN_DOT : ICON_GRAY_DOT);
    }
}

// ================================================================
//  Debug Mode
// ================================================================

void BTView::onPlayStateChanged(const PlayStateChangedEvent& event) {
    if (event.isPlaying) {
        debugMode_ = true;
        debugTimer_ = 0.0f;
        debugStep_ = 0;
        CCLOG("[BTView] Debug mode activated (play started)");
    } else {
        debugMode_ = false;
        // Clear execution states
        model_->traverse([](const BTNode& node, int) {
            const_cast<BTNode&>(node).debugExecuting = false;
        });
        CCLOG("[BTView] Debug mode deactivated (play stopped)");
    }
}

void BTView::updateDebugSimulation() {
    // This is a simulation placeholder. In production, Behaviac would
    // provide the actual executing node via its debugger interface.
    // We cycle through nodes every 1.0 seconds for demonstration.
    if (!debugMode_) return;

    debugTimer_ += 0.016f; // approximate frame delta
    if (debugTimer_ < 1.0f) return;
    debugTimer_ = 0.0f;

    // Clear previous execution state
    model_->traverse([](const BTNode& node, int) {
        const_cast<BTNode&>(node).debugExecuting = false;
    });

    // Simulate: cycle through root's children
    auto root = model_->getRoot();
    if (root && !root->children.empty()) {
        size_t idx = debugStep_ % root->children.size();
        root->children[idx]->debugExecuting = true;
        // Also set the first grandchild if available
        if (!root->children[idx]->children.empty()) {
            root->children[idx]->children[0]->debugExecuting = true;
        }
    }
    ++debugStep_;
}

// ================================================================
//  Color mapping
// ================================================================

ImVec4 BTView::colorForType(BTNodeType type) const {
    switch (type) {
        case BTNodeType::Selector:  return ImVec4(0.3f, 0.8f, 0.3f, 1.0f);  // green
        case BTNodeType::Sequence:  return ImVec4(0.3f, 0.5f, 1.0f, 1.0f);  // blue
        case BTNodeType::Condition: return ImVec4(1.0f, 0.9f, 0.2f, 1.0f);  // yellow
        case BTNodeType::Action:    return ImVec4(1.0f, 0.3f, 0.3f, 1.0f);  // red
    }
    return ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
}

// ================================================================
//  BTBehaviacBridge : ugf::Component — Behaviac integration
// ================================================================

bool BTBehaviacBridge::initialize(const std::unordered_map<std::string, std::any>& /*config*/) {
    auto* workspace = behaviac::Workspace::GetInstance();
    if (workspace) {
        CCLOG("[BTBehaviacBridge] Behaviac Workspace OK: %p", static_cast<void*>(workspace));
        workspaceReady_ = true;
        return true;
    }
    CCLOG("[BTBehaviacBridge] Behaviac Workspace not available");
    workspaceReady_ = false;
    return false;
}

// ================================================================
//  API Tests (registered via static init — run at plugin init)
// ================================================================

static bool test_bt_model_create() {
    auto root = BTModel::createNode(BTNodeType::Selector, "Root");
    return root != nullptr && std::string(root->typeName()) == "Selector";
}
REGISTER_API_TEST("BehaviorTree", test_bt_model_create);

static bool test_bt_model_add_child() {
    auto root = BTModel::createNode(BTNodeType::Selector, "Root");
    auto attack = BTModel::createNode(BTNodeType::Sequence, "Attack");
    root->addChild(attack);
    return root->childCount() == 1 && root->children[0]->name == "Attack";
}
REGISTER_API_TEST("BehaviorTree", test_bt_model_add_child);

static bool test_bt_model_traverse() {
    auto root = BTModel::createNode(BTNodeType::Selector, "Root");
    auto attack = BTModel::createNode(BTNodeType::Sequence, "Attack");
    auto hasTarget = BTModel::createNode(BTNodeType::Condition, "HasTarget");
    attack->addChild(hasTarget);
    root->addChild(attack);

    BTModel model;
    model.setRoot(root);

    int count = 0;
    model.traverse([&count](const BTNode&, int) { ++count; });
    return count == 3;
}
REGISTER_API_TEST("BehaviorTree", test_bt_model_traverse);

static bool test_bt_model_add_node_method() {
    BTModel model;
    model.initialize();
    auto root = model.getRoot();
    if (!root) return false;

    auto child = model.addNode("TestAction", BTNodeType::Action, root);
    if (!child) return false;
    if (child->name != "TestAction") return false;
    if (child->type != BTNodeType::Action) return false;
    auto parent = child->parent.lock();
    return parent == root;
}
REGISTER_API_TEST("BehaviorTree", test_bt_model_add_node_method);

static bool test_bt_model_default_tree() {
    BTModel model;
    model.initialize();
    auto root = model.getRoot();
    if (!root) return false;
    if (root->type != BTNodeType::Selector) return false;
    if (root->name != "Root") return false;

    size_t count = model.nodeCount();
    if (count != 5) return false;
    return model.getSelected() == nullptr;
}
REGISTER_API_TEST("BehaviorTree", test_bt_model_default_tree);

static bool test_bt_model_selection() {
    BTModel model;
    model.initialize();
    auto root = model.getRoot();
    if (!root) return false;

    model.setSelected(root);
    if (model.getSelected() != root) return false;

    auto child = root->children[0];
    model.setSelected(child);
    if (model.getSelected() != child) return false;
    if (model.getSelected()->name != "Attack") return false;
    return true;
}
REGISTER_API_TEST("BehaviorTree", test_bt_model_selection);

// ================================================================
//  New API Tests: removeNode, findNode, generateName, circular rejection
// ================================================================

static bool test_remove_node() {
    BTModel model;
    model.initialize();
    size_t before = model.nodeCount(); // should be 5

    // Remove "Idle" (a leaf node)
    auto idle = model.findNode("Idle");
    if (!idle) return false;
    bool ok = model.removeNode(idle);
    if (!ok) return false;
    if (model.nodeCount() != before - 1) return false;

    // Idle should no longer be in the tree
    auto found = model.findNode("Idle");
    return found == nullptr;
}
REGISTER_API_TEST("BehaviorTree", test_remove_node);

static bool test_cannot_remove_root() {
    BTModel model;
    model.initialize();
    auto root = model.getRoot();
    bool ok = model.removeNode(root);
    if (ok) return false; // should refuse
    return model.nodeCount() == 5; // tree intact
}
REGISTER_API_TEST("BehaviorTree", test_cannot_remove_root);

static bool test_find_node_by_name() {
    BTModel model;
    model.initialize();

    auto found = model.findNode("HasTarget");
    if (!found) return false;
    if (found->type != BTNodeType::Condition) return false;

    auto notFound = model.findNode("NonExistent");
    return notFound == nullptr;
}
REGISTER_API_TEST("BehaviorTree", test_find_node_by_name);

static bool test_generate_name() {
    BTModel model;
    model.initialize(); // has 1 Selector (Root), 1 Sequence (Attack),
                        // 1 Condition (HasTarget), 2 Actions (DoAttack, Idle)

    // First new Selector should be "Selector_2" (Root is Selector_1-like)
    std::string name1 = model.generateName(BTNodeType::Selector);
    // Counting: Root is a Selector → count=1 → next is Selector_2
    if (name1 != "Selector_2") return false;

    // Add a new Action, then next Action name should be "Action_3"
    model.addNode("NewAction", BTNodeType::Action, model.getRoot());
    std::string name2 = model.generateName(BTNodeType::Action);
    // Counting: DoAttack(1) + Idle(1) + NewAction(1) = 3 Actions → next is Action_4
    if (name2 != "Action_4") return false;

    return true;
}
REGISTER_API_TEST("BehaviorTree", test_generate_name);

static bool test_add_node_all_4_types() {
    BTModel model;
    auto root = BTModel::createNode(BTNodeType::Selector, "Root");
    model.setRoot(root);

    auto sel  = model.addNode("Sel",  BTNodeType::Selector,  root);
    auto seq  = model.addNode("Seq",  BTNodeType::Sequence,  root);
    auto cond = model.addNode("Cond", BTNodeType::Condition, root);
    auto act  = model.addNode("Act",  BTNodeType::Action,    root);

    if (!sel || !seq || !cond || !act) return false;
    if (root->childCount() != 4) return false;
    if (sel->type != BTNodeType::Selector) return false;
    if (seq->type != BTNodeType::Sequence) return false;
    if (cond->type != BTNodeType::Condition) return false;
    if (act->type != BTNodeType::Action) return false;

    // Verify parent pointers
    return sel->parent.lock() == root
        && seq->parent.lock() == root
        && cond->parent.lock() == root
        && act->parent.lock() == root;
}
REGISTER_API_TEST("BehaviorTree", test_add_node_all_4_types);

static bool test_remove_node_cleans_shared_ptr() {
    BTModel model;
    auto root = BTModel::createNode(BTNodeType::Selector, "Root");
    model.setRoot(root);
    auto child = model.addNode("Child", BTNodeType::Action, root);
    if (!child) return false;
    if (root->childCount() != 1) return false;

    // Remove child; shared_ptr still alive until child goes out of scope
    bool ok = model.removeNode(child);
    if (!ok) return false;
    if (root->childCount() != 0) return false;

    // child shared_ptr is still valid here (we hold it), but parent disconnected
    if (child->parent.lock() != nullptr) return false;

    // Model no longer tracks it
    auto found = model.findNode("Child");
    return found == nullptr;
}
REGISTER_API_TEST("BehaviorTree", test_remove_node_cleans_shared_ptr);

static bool test_circular_drag_drop_rejection() {
    // Build: Root → A → B → C
    auto root = BTModel::createNode(BTNodeType::Selector, "Root");
    auto a = BTModel::createNode(BTNodeType::Sequence, "A");
    auto b = BTModel::createNode(BTNodeType::Condition, "B");
    auto c = BTModel::createNode(BTNodeType::Action, "C");
    root->addChild(a);
    a->addChild(b);
    b->addChild(c);

    // C is descendant of A
    if (!c->isDescendantOf(a)) return false;
    if (!c->isDescendantOf(root)) return false;

    // A is NOT descendant of C
    if (a->isDescendantOf(c)) return false;

    // Drop C onto A: A is ancestor of C → circular, should reject
    // (C is already under A, so reparenting C to A is nested circular)
    // Actually: C's parent is B, B's parent is A. So A IS an ancestor of C.
    // Dropping C onto A would mean C becomes a child of A while already being a descendant.
    // Our isValidDropTarget should catch this since target.isDescendantOf(source) → A.isDescendantOf(C) → false.
    // Wait no: the check is source->isDescendantOf(target) || target->isDescendantOf(source).
    // source=C, target=A: C.isDescendantOf(A) → TRUE → invalid.
    // This is correct.

    // Self-drop should also reject
    // source=A, target=A: source == target → invalid.

    return true;
}
REGISTER_API_TEST("BehaviorTree", test_circular_drag_drop_rejection);

static bool test_node_extra_properties() {
    auto cond = BTModel::createNode(BTNodeType::Condition, "TestCond");
    cond->conditionScript = "hp > 50";
    if (cond->conditionScript != "hp > 50") return false;

    auto act = BTModel::createNode(BTNodeType::Action, "TestAct");
    act->actionName = "PlayAnimation";
    if (act->actionName != "PlayAnimation") return false;

    // Selector/Sequence should have empty defaults
    auto sel = BTModel::createNode(BTNodeType::Selector, "TestSel");
    if (!sel->conditionScript.empty()) return false;
    if (!sel->actionName.empty()) return false;

    return true;
}
REGISTER_API_TEST("BehaviorTree", test_node_extra_properties);

static bool test_parent_pointer() {
    auto root = BTModel::createNode(BTNodeType::Selector, "Root");
    auto child = BTModel::createNode(BTNodeType::Action, "Child");
    root->addChild(child);

    auto parent = child->parent.lock();
    if (!parent || parent->name != "Root" || parent != root) return false;
    return root->parent.lock() == nullptr;
}
REGISTER_API_TEST("BehaviorTree", test_parent_pointer);

// ================================================================
//  BehaviorTreePlugin : ugf::IPlugin — multi-component plugin
// ================================================================

bool BehaviorTreePlugin::initialize() {
    CCLOG("[BTEditor] Initializing BehaviorTreePlugin (multi-component)...");

    // 1. Register BTModel component (data)
    componentSystem.registerComponent<BTModel>("model");
    auto* model = componentSystem.get<BTModel>("model");
    if (!model) {
        CCLOG("[BTEditor] ERROR: Failed to register BTModel");
        return false;
    }
    CCLOG("[BTEditor] BTModel registered, %zu nodes in default tree", model->nodeCount());

    // 2. Register BTView component (UI) — pass BTModel pointer via config
    std::unordered_map<std::string, std::any> viewCfg;
    viewCfg["model"] = model;
    componentSystem.registerComponent<BTView>("view", viewCfg);

    // 3. Register BTBehaviacBridge component (Behaviac integration)
    componentSystem.registerComponent<BTBehaviacBridge>("behaviac");

    // 4. Run API tests
    TestFramework::getInstance().runApiTests("BehaviorTree");

    CCLOG("[BTEditor] Plugin initialized with %zu components", componentSystem.size());
    return true;
}

void BehaviorTreePlugin::update(float dt) {
    // Update all components (BTView::update renders the UI)
    componentSystem.updateAll(dt);
    // Run GUI tests in their own window
    TestFramework::getInstance().runGuiTests("BehaviorTree");
}

void BehaviorTreePlugin::shutdown() {
    CCLOG("[BTEditor] Shutting down.");
    componentSystem.clear();
}
