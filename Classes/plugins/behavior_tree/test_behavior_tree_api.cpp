// test_behavior_tree_api.cpp - API and GUI tests for BehaviorTree plugin
//
// Tests are registered at static init time and run by the plugin:
//   API tests: run once at plugin initialization (console PASS/FAIL)
//   GUI tests: displayed in a "GUI Test Runner" ImGui window each frame
//
// Coverage: node CRUD, selection, parent pointers, remove/cleanup,
//           find by name, generate name, circular drag-drop rejection,
//           extra properties (conditionScript, actionName)
#include "BehaviorTreePlugin.h"
#include "tests/TestFramework.h"
#include "imgui.h"
#include <string>

// Unicode icons for debug mode indicators
#ifndef ICON_GREEN_DOT
#define ICON_GREEN_DOT "\xE2\x97\x8F"  // U+25CF
#endif
#ifndef ICON_GRAY_DOT
#define ICON_GRAY_DOT  "\xE2\x97\x8B"  // U+25CB
#endif

// ================================================================
//  API Tests
// ================================================================

// --- Node creation with all 4 types ---
static bool test_create_all_node_types() {
    auto sel  = BTModel::createNode(BTNodeType::Selector,  "S");
    auto seq  = BTModel::createNode(BTNodeType::Sequence,  "Q");
    auto cond = BTModel::createNode(BTNodeType::Condition, "C");
    auto act  = BTModel::createNode(BTNodeType::Action,    "A");
    return sel != nullptr && seq != nullptr && cond != nullptr && act != nullptr
        && std::string(sel->typeName())  == "Selector"
        && std::string(seq->typeName())  == "Sequence"
        && std::string(cond->typeName()) == "Condition"
        && std::string(act->typeName())  == "Action";
}
REGISTER_API_TEST("BehaviorTree", test_create_all_node_types);

// --- addNode with all 4 types ---
static bool test_add_node_all_types() {
    BTModel model;
    auto root = BTModel::createNode(BTNodeType::Selector, "Root");
    model.setRoot(root);

    auto sel  = model.addNode("Sel",  BTNodeType::Selector,  root);
    auto seq  = model.addNode("Seq",  BTNodeType::Sequence,  root);
    auto cond = model.addNode("Cond", BTNodeType::Condition, root);
    auto act  = model.addNode("Act",  BTNodeType::Action,    root);

    if (!sel || !seq || !cond || !act) return false;
    if (root->childCount() != 4) return false;
    if (sel->type  != BTNodeType::Selector)  return false;
    if (seq->type  != BTNodeType::Sequence)  return false;
    if (cond->type != BTNodeType::Condition) return false;
    if (act->type  != BTNodeType::Action)    return false;

    // Verify parent pointers
    return sel->parent.lock()  == root
        && seq->parent.lock()  == root
        && cond->parent.lock() == root
        && act->parent.lock()  == root;
}
REGISTER_API_TEST("BehaviorTree", test_add_node_all_types);

// --- removeNode detaches from parent; shared_ptr cleans up ---
static bool test_remove_node_cleanup() {
    BTModel model;
    auto root = BTModel::createNode(BTNodeType::Selector, "Root");
    model.setRoot(root);

    auto child = model.addNode("Child", BTNodeType::Action, root);
    if (!child || root->childCount() != 1) return false;

    // Remove detaches from parent
    bool ok = model.removeNode(child);
    if (!ok) return false;
    if (root->childCount() != 0) return false;

    // Parent link is broken
    if (child->parent.lock() != nullptr) return false;

    // Model no longer finds it via traversal
    auto found = model.findNode("Child");
    return found == nullptr;
}
REGISTER_API_TEST("BehaviorTree", test_remove_node_cleanup);

// --- Cannot remove root ---
static bool test_cannot_remove_root_node() {
    BTModel model;
    model.initialize(); // default tree has 5 nodes
    auto root = model.getRoot();
    if (!root) return false;

    bool ok = model.removeNode(root);
    if (ok) return false; // should refuse
    return model.nodeCount() == 5;
}
REGISTER_API_TEST("BehaviorTree", test_cannot_remove_root_node);

// --- findNode by name ---
static bool test_find_node_deep_search() {
    BTModel model;
    model.initialize();

    auto found = model.findNode("HasTarget");
    if (!found) return false;
    if (found->type != BTNodeType::Condition) return false;

    // Deep search
    auto deep = model.findNode("DoAttack");
    if (!deep || deep->type != BTNodeType::Action) return false;

    // Non-existent node
    return model.findNode("NonExistent") == nullptr;
}
REGISTER_API_TEST("BehaviorTree", test_find_node_deep_search);

// --- Default tree structure ---
static bool test_default_tree() {
    BTModel model;
    model.initialize();

    auto root = model.getRoot();
    if (!root || root->type != BTNodeType::Selector || root->name != "Root")
        return false;
    if (model.nodeCount() != 5) return false;
    return model.getSelected() == nullptr;
}
REGISTER_API_TEST("BehaviorTree", test_default_tree);

// --- Selection ---
static bool test_selection_state() {
    BTModel model;
    model.initialize();

    auto root = model.getRoot();
    model.setSelected(root);
    if (model.getSelected() != root) return false;

    model.setSelected(root->children[0]); // Attack
    if (model.getSelected()->name != "Attack") return false;

    // Deselect
    model.setSelected(nullptr);
    return model.getSelected() == nullptr;
}
REGISTER_API_TEST("BehaviorTree", test_selection_state);

// --- Parent pointer correctness ---
static bool test_parent_weak_ptr() {
    auto root = BTModel::createNode(BTNodeType::Selector, "Root");
    auto child = BTModel::createNode(BTNodeType::Action, "Child");
    root->addChild(child);

    auto parent = child->parent.lock();
    if (!parent || parent->name != "Root" || parent != root) return false;

    // Root has no parent
    return root->parent.lock() == nullptr;
}
REGISTER_API_TEST("BehaviorTree", test_parent_weak_ptr);

// --- Circular drag-drop rejection (isDescendantOf) ---
static bool test_circular_drop_rejection() {
    // Build: Root → A → B → C
    auto root = BTModel::createNode(BTNodeType::Selector, "Root");
    auto a = BTModel::createNode(BTNodeType::Sequence, "A");
    auto b = BTModel::createNode(BTNodeType::Condition, "B");
    auto c = BTModel::createNode(BTNodeType::Action, "C");
    root->addChild(a);
    a->addChild(b);
    b->addChild(c);

    // C is a descendant of A (A → B → C)
    if (!c->isDescendantOf(a)) return false;
    // C is also a descendant of Root
    if (!c->isDescendantOf(root)) return false;
    // A is NOT a descendant of C
    if (a->isDescendantOf(c)) return false;
    // Root is NOT a descendant of anything
    if (root->isDescendantOf(a)) return false;

    // Self-check: a node is not a descendant of itself
    // (isDescendantOf follows parent chain, not self-inclusive)
    if (c->isDescendantOf(c)) return false;

    return true;
}
REGISTER_API_TEST("BehaviorTree", test_circular_drop_rejection);

// --- Extra node properties (conditionScript, actionName) ---
static bool test_extra_properties() {
    auto cond = BTModel::createNode(BTNodeType::Condition, "TestCond");
    cond->conditionScript = "hp > 50";
    if (cond->conditionScript != "hp > 50") return false;

    auto act = BTModel::createNode(BTNodeType::Action, "TestAct");
    act->actionName = "PlayAnimation";
    if (act->actionName != "PlayAnimation") return false;

    // Selector/Sequence should have empty defaults
    auto sel = BTModel::createNode(BTNodeType::Selector, "TestSel");
    auto seq = BTModel::createNode(BTNodeType::Sequence, "TestSeq");
    if (!sel->conditionScript.empty()) return false;
    if (!sel->actionName.empty()) return false;
    if (!seq->conditionScript.empty()) return false;
    if (!seq->actionName.empty()) return false;

    return true;
}
REGISTER_API_TEST("BehaviorTree", test_extra_properties);

// --- Generate unique name ---
static bool test_generate_unique_name() {
    BTModel model;
    model.initialize();
    // Default: 1 Selector (Root), 1 Sequence (Attack),
    //           1 Condition (HasTarget), 2 Actions (DoAttack, Idle)

    std::string selName  = model.generateName(BTNodeType::Selector);
    std::string seqName  = model.generateName(BTNodeType::Sequence);
    std::string condName = model.generateName(BTNodeType::Condition);
    std::string actName  = model.generateName(BTNodeType::Action);

    if (selName != "Selector_2") return false;  // 1 existing → next is 2
    if (seqName != "Sequence_2") return false;
    if (condName != "Condition_2") return false;
    if (actName != "Action_3") return false;    // 2 existing → next is 3

    return true;
}
REGISTER_API_TEST("BehaviorTree", test_generate_unique_name);

// ================================================================
//  GUI Tests — render descriptive test info in "GUI Test Runner" window
// ================================================================

static void test_bt_tree_display() {
    ImGui::TextUnformatted("Behavior Tree Display Test");
    ImGui::BulletText("Open the \"Behavior Tree\" window to verify the tree view.");
    ImGui::BulletText("Expected node colors:");
    ImGui::TextColored(ImVec4(0.3f, 0.8f, 0.3f, 1.0f), "    Selector (green)");
    ImGui::TextColored(ImVec4(0.3f, 0.5f, 1.0f, 1.0f), "    Sequence (blue)");
    ImGui::TextColored(ImVec4(1.0f, 0.9f, 0.2f, 1.0f), "    Condition (yellow)");
    ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "    Action (red)");
    ImGui::BulletText("Default tree structure:");
    ImGui::BulletText("  Root (Selector) -> Attack (Sequence) -> HasTarget (Condition)");
    ImGui::BulletText("                               \\-> DoAttack (Action)");
    ImGui::BulletText("                  -> Idle (Action)");
}
REGISTER_GUI_TEST("BehaviorTree", test_bt_tree_display);

static void test_bt_context_menu() {
    ImGui::TextUnformatted("Context Menu Test");
    ImGui::BulletText("Right-click any node in the Behavior Tree window.");
    ImGui::BulletText("The context menu offers:");
    ImGui::BulletText("  Add Child > (Selector, Sequence, Condition, Action)");
    ImGui::BulletText("  Delete (disabled for root)");
    ImGui::BulletText("  Rename (opens a modal dialog)");
    ImGui::Spacing();
    ImGui::TextDisabled("Try: right-click \"Idle\" → Add Child → Action");
    ImGui::TextDisabled("     Then right-click the new node → Delete");
}
REGISTER_GUI_TEST("BehaviorTree", test_bt_context_menu);

static void test_bt_drag_drop() {
    ImGui::TextUnformatted("Drag & Drop Test");
    ImGui::BulletText("Drag any non-root node to another node.");
    ImGui::BulletText("During drag: source node dims, target highlights.");
    ImGui::BulletText("Invalid targets (self, descendants) are rejected.");
    ImGui::BulletText("On drop: the node is reparented as a child of the target.");
    ImGui::Spacing();
    ImGui::TextDisabled("Try: drag \"Idle\" onto \"Attack\" to reparent.");
    ImGui::TextDisabled("     Then check the console for reparenting log.");
}
REGISTER_GUI_TEST("BehaviorTree", test_bt_drag_drop);

static void test_bt_properties() {
    ImGui::TextUnformatted("Properties Panel Test");
    ImGui::BulletText("Click any node in the tree to select it.");
    ImGui::BulletText("The right-side Properties panel shows:");
    ImGui::BulletText("  Name — editable text field");
    ImGui::BulletText("  Type — read-only combo box");
    ImGui::BulletText("  Condition Script — only for Condition nodes");
    ImGui::BulletText("  Action Name — only for Action nodes");
    ImGui::Spacing();
    ImGui::TextDisabled("Try: select \"HasTarget\" → edit Condition Script");
    ImGui::TextDisabled("     select \"DoAttack\"  → edit Action Name");
}
REGISTER_GUI_TEST("BehaviorTree", test_bt_properties);

static void test_bt_debug_mode() {
    ImGui::TextUnformatted("Debug Mode Test");
    ImGui::BulletText("Toggle \"Debug Mode\" checkbox in the BT toolbar.");
    ImGui::BulletText("Green dot icon beside nodes marked as executing.");
    ImGui::BulletText("Gray dot icon beside idle composite nodes.");
    ImGui::BulletText("Toolbar shows current running node names.");
    ImGui::BulletText("Properties panel shows debug state of selected node.");
    ImGui::BulletText("Debug mode auto-activates on PlayStateChanged(playing=true).");
}
REGISTER_GUI_TEST("BehaviorTree", test_bt_debug_mode);
