// BehaviorTreePlugin.cpp - Behavior Tree Editor Plugin implementation
// Multi-component architecture: BTModel (data) + BTView (UI) + BTBehaviacBridge (integration)
#include "BehaviorTreePlugin.h"
#include "behaviac/behaviac.h"
#include "imgui.h"
#include "cocos2d.h"
#include "tests/TestFramework.h"

// ================================================================
//  BTView : ugf::Component — ImGui tree visualization
// ================================================================

void BTView::update(float /*deltaTime*/) {
    if (!model_) return;
    renderWindow();
}

void BTView::renderWindow() {
    ImGui::SetNextWindowSize(ImVec2(350, 450), ImGuiCond_FirstUseEver);

    if (!ImGui::Begin("Behavior Tree")) {
        ImGui::End();
        return;
    }

    // Display selected node info
    auto selected = model_->getSelected();
    if (selected) {
        ImVec4 selColor = colorForType(selected->type);
        ImGui::TextColored(selColor, "Selected: %s (%s)",
                           selected->name.c_str(), selected->typeName());
    } else {
        ImGui::TextDisabled("No node selected");
    }

    ImGui::Separator();

    // Render the tree
    auto root = model_->getRoot();
    if (root) {
        renderNode(root);
    } else {
        ImGui::TextDisabled("(empty tree)");
    }

    ImGui::End();
}

void BTView::renderNode(const std::shared_ptr<BTNode>& node) {
    if (!node) return;

    ImGui::PushID(static_cast<const void*>(node.get()));

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;
    if (node->children.empty()) {
        flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_Bullet;
    }
    if (model_->getSelected() == node) {
        flags |= ImGuiTreeNodeFlags_Selected;
    }
    // Default-open the root node
    if (!node->parent.lock()) {
        flags |= ImGuiTreeNodeFlags_DefaultOpen;
    }

    // Color-code by node type
    ImVec4 color = colorForType(node->type);
    ImGui::PushStyleColor(ImGuiCol_Text, color);

    char label[256];
    snprintf(label, sizeof(label), "%s (%s)", node->name.c_str(), node->typeName());
    bool opened = ImGui::TreeNodeEx(label, flags);

    ImGui::PopStyleColor();

    // Handle click — select the node
    if (ImGui::IsItemClicked()) {
        model_->setSelected(node);
        CCLOG("[BTEditor] Selected: %s (%s)", node->name.c_str(), node->typeName());
    }

    if (opened) {
        for (auto& child : node->children) {
            renderNode(child);
        }
        ImGui::TreePop();
    }

    ImGui::PopID();
}

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
    CCLOG("[BTEditor] Behaviac Workspace OK: %p", static_cast<void*>(workspace));
    return true;
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

    // Add a child via model.addNode(name, type, parent)
    auto child = model.addNode("TestAction", BTNodeType::Action, root);
    if (!child) return false;
    if (child->name != "TestAction") return false;
    if (child->type != BTNodeType::Action) return false;
    // Verify parent pointer
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

    // Expected: Root -> Attack -> HasTarget, DoAttack; Root -> Idle
    size_t count = model.nodeCount();
    if (count != 5) return false; // Root, Attack, HasTarget, DoAttack, Idle

    // Check selection defaults to null
    return model.getSelected() == nullptr;
}
REGISTER_API_TEST("BehaviorTree", test_bt_model_default_tree);

static bool test_bt_model_selection() {
    BTModel model;
    model.initialize();
    auto root = model.getRoot();
    if (!root) return false;

    // Select root
    model.setSelected(root);
    if (model.getSelected() != root) return false;

    // Select a child
    auto child = root->children[0]; // Attack
    model.setSelected(child);
    if (model.getSelected() != child) return false;
    if (model.getSelected()->name != "Attack") return false;

    return true;
}
REGISTER_API_TEST("BehaviorTree", test_bt_model_selection);

// ================================================================
//  GUI Tests (registered via static init)
// ================================================================

static void test_gui_behavior_tree_info() {
    ImGui::Text("Behavior Tree Plugin - Multi-Component Architecture");
    ImGui::BulletText("BTModel: data component (tree structure, selection)");
    ImGui::BulletText("BTView: ImGui component (color-coded tree rendering)");
    ImGui::BulletText("BTBehaviacBridge: Behaviac workspace integration");
    ImGui::BulletText("Node colors: Selector=green, Sequence=blue, Condition=yellow, Action=red");
}
REGISTER_GUI_TEST("BehaviorTree", test_gui_behavior_tree_info);

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

void BehaviorTreePlugin::shutdown() {
    CCLOG("[BTEditor] Shutting down.");
    componentSystem.clear();
}
