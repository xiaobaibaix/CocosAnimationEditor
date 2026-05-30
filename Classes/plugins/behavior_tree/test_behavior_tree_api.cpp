// test_behavior_tree_api.cpp - API and GUI tests for BehaviorTree plugin
// Tests cover BTModel (data), BTView (UI), and BTBehaviacBridge (Behaviac integration)
#include "BehaviorTreePlugin.h"
#include "tests/TestFramework.h"
#include "imgui.h"

// ================================================================
//  BTModel API Tests — compile-time registered, run at plugin init
// ================================================================

static bool test_create_node() {
    auto node = BTModel::createNode(BTNodeType::Selector, "Root");
    return node != nullptr && std::string(node->typeName()) == "Selector";
}
REGISTER_API_TEST("BehaviorTree", test_create_node);

static bool test_add_child() {
    auto root = BTModel::createNode(BTNodeType::Selector, "Root");
    auto attack = BTModel::createNode(BTNodeType::Sequence, "Attack");
    root->addChild(attack);
    return root->childCount() == 1 && root->children[0]->name == "Attack";
}
REGISTER_API_TEST("BehaviorTree", test_add_child);

static bool test_traverse() {
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
REGISTER_API_TEST("BehaviorTree", test_traverse);

static bool test_add_node_method() {
    BTModel model;
    model.initialize();
    auto root = model.getRoot();
    if (!root) return false;

    auto child = model.addNode("TestAction", BTNodeType::Action, root);
    if (!child) return false;
    if (child->name != "TestAction") return false;

    // Verify parent weak_ptr
    auto parent = child->parent.lock();
    return parent == root;
}
REGISTER_API_TEST("BehaviorTree", test_add_node_method);

static bool test_default_tree_structure() {
    BTModel model;
    model.initialize();

    auto root = model.getRoot();
    if (!root || root->type != BTNodeType::Selector || root->name != "Root")
        return false;

    // Should have 5 nodes: Root, Attack, HasTarget, DoAttack, Idle
    if (model.nodeCount() != 5) return false;

    // Selection starts null
    return model.getSelected() == nullptr;
}
REGISTER_API_TEST("BehaviorTree", test_default_tree_structure);

static bool test_selection() {
    BTModel model;
    model.initialize();

    auto root = model.getRoot();
    model.setSelected(root);
    if (model.getSelected() != root) return false;

    // Select Attack (first child)
    model.setSelected(root->children[0]);
    if (model.getSelected()->name != "Attack") return false;

    return true;
}
REGISTER_API_TEST("BehaviorTree", test_selection);

static bool test_parent_pointer() {
    auto root = BTModel::createNode(BTNodeType::Selector, "Root");
    auto child = BTModel::createNode(BTNodeType::Action, "Child");
    root->addChild(child);

    auto parent = child->parent.lock();
    if (!parent) return false;
    if (parent->name != "Root") return false;
    if (parent != root) return false;

    // Root should have no parent
    return root->parent.lock() == nullptr;
}
REGISTER_API_TEST("BehaviorTree", test_parent_pointer);

// ================================================================
//  GUI Tests — render interactive test info inside ImGui window
// ================================================================

static void test_plugin_info() {
    ImGui::Text("Behavior Tree Plugin loaded (multi-component architecture).");
    ImGui::BulletText("BTModel: stores tree data (Component)");
    ImGui::BulletText("BTView: renders tree hierarchy in ImGui (Component)");
    ImGui::BulletText("BTBehaviacBridge: connects to Behaviac runtime (Component)");
    ImGui::BulletText("Plugin = IPlugin + ComponentSystem with 3 components");
    ImGui::BulletText("Node colors: Selector=green, Sequence=blue, Condition=yellow, Action=red");
}
REGISTER_GUI_TEST("BehaviorTree", test_plugin_info);
