// test_scene_tree_api.cpp - SceneTreeModel 额外 API 测试
#include "imgui.h"
#include "tests/TestFramework.h"
#include "plugins/scene_tree/SceneTreePlugin.h"

static bool test_find_nonexistent_returns_null() {
    SceneTreeModel model;
    return model.findNode("NonExistent") == nullptr;
}
REGISTER_API_TEST("SceneTree", test_find_nonexistent_returns_null);

static bool test_add_nested_nodes() {
    SceneTreeModel model;
    TreeNode* child = model.addNode("Child", model.getRoot());
    model.addNode("Grandchild", child);
    if (model.getNodeCount() != 6) return false;
    return model.findNode("Grandchild") != nullptr;
}
REGISTER_API_TEST("SceneTree", test_add_nested_nodes);

static bool test_selected_node_removed_clears_selection() {
    SceneTreeModel model;
    model.selectNode("Bone_Arm");
    model.removeNode("Bone_Arm");
    return model.getSelectedNode() == nullptr;
}
REGISTER_API_TEST("SceneTree", test_selected_node_removed_clears_selection);

// --- New: Rename / Duplicate / Move tests ---

static bool test_rename_node() {
    SceneTreeModel model;
    if (!model.renameNode("Bone_Arm", "Arm_Bone")) return false;
    if (model.findNode("Bone_Arm") != nullptr) return false;
    if (model.findNode("Arm_Bone") == nullptr) return false;
    return true;
}
REGISTER_API_TEST("SceneTree", test_rename_node);

static bool test_rename_rejects_duplicate_name() {
    SceneTreeModel model;
    // Renaming Bone_Arm to Bone_Leg should fail (name collision)
    if (model.renameNode("Bone_Arm", "Bone_Leg")) return false;
    return model.findNode("Bone_Arm") != nullptr;
}
REGISTER_API_TEST("SceneTree", test_rename_rejects_duplicate_name);

static bool test_rename_root_rejected() {
    SceneTreeModel model;
    return !model.renameNode("SceneRoot", "NewRoot");
}
REGISTER_API_TEST("SceneTree", test_rename_root_rejected);

static bool test_duplicate_node() {
    SceneTreeModel model;
    int countBefore = model.getNodeCount();
    if (!model.duplicateNode("Bone_Arm")) return false;
    if (model.getNodeCount() != countBefore + 1) return false;
    TreeNode* dup = model.findNode("Bone_Arm (Copy)");
    if (!dup) return false;
    // Duplicate should be sibling of source
    return dup->parent == model.findNode("Bone_Arm")->parent;
}
REGISTER_API_TEST("SceneTree", test_duplicate_node);

static bool test_move_node_reparent() {
    SceneTreeModel model;
    TreeNode* arm = model.findNode("Bone_Arm");
    TreeNode* hero = model.findNode("Sprite_Hero");
    if (!model.moveNode("Bone_Arm", "Sprite_Hero", InsertPosition::AsChild))
        return false;
    if (arm->parent != hero) return false;
    // Root should no longer contain Bone_Arm directly
    for (auto* c : model.getRoot()->children) {
        if (c->name == "Bone_Arm") return false;
    }
    // Hero should now contain Bone_Arm
    for (auto* c : hero->children) {
        if (c->name == "Bone_Arm") return true;
    }
    return false;
}
REGISTER_API_TEST("SceneTree", test_move_node_reparent);

static bool test_move_node_self_drop() {
    SceneTreeModel model;
    return !model.moveNode("Bone_Arm", "Bone_Arm", InsertPosition::AsChild);
}
REGISTER_API_TEST("SceneTree", test_move_node_self_drop);

static bool test_move_node_descendant_drop() {
    SceneTreeModel model;
    // Make Bone_Arm a child of Sprite_Hero
    model.moveNode("Bone_Arm", "Sprite_Hero", InsertPosition::AsChild);
    // Now try to move Sprite_Hero into Bone_Arm (circular)
    return !model.moveNode("Sprite_Hero", "Bone_Arm", InsertPosition::AsChild);
}
REGISTER_API_TEST("SceneTree", test_move_node_descendant_drop);

static bool test_context_menu_delete() {
    SceneTreeModel model;
    // Create a parent with one child, then delete the parent —
    // both should be removed and nodeCount updated correctly.
    TreeNode* parent = model.addNode("Parent", model.getRoot());
    model.addNode("Child", parent);
    int count = model.getNodeCount(); // 4 default + 2 new = 6
    if (!model.removeNode("Parent")) return false;
    if (model.getNodeCount() != count - 2) return false;
    if (model.findNode("Parent") != nullptr) return false;
    return model.findNode("Child") == nullptr;
}
REGISTER_API_TEST("SceneTree", test_context_menu_delete);

static bool test_move_node_before_sibling() {
    SceneTreeModel model;
    // Move Bone_Leg before Bone_Arm
    if (!model.moveNode("Bone_Leg", "Bone_Arm", InsertPosition::BeforeSibling))
        return false;
    // Verify order: Bone_Leg should come before Bone_Arm in root's children
    auto& children = model.getRoot()->children;
    int legIdx = -1, armIdx = -1;
    for (size_t i = 0; i < children.size(); ++i) {
        if (children[i]->name == "Bone_Leg") legIdx = static_cast<int>(i);
        if (children[i]->name == "Bone_Arm") armIdx = static_cast<int>(i);
    }
    return legIdx >= 0 && armIdx >= 0 && legIdx < armIdx;
}
REGISTER_API_TEST("SceneTree", test_move_node_before_sibling);
