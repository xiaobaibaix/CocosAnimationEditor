// test_scene_tree_api.cpp - SceneTreeModel 的额外 API 单元测试
// 核心测试在 SceneTreePlugin.cpp 中注册，此文件补充边界情况测试

#include "imgui.h"
#include "tests/TestFramework.h"
#include "plugins/scene_tree/SceneTreePlugin.h"

static bool test_find_nonexistent_returns_null() {
    SceneTreeModel model;
    TreeNode* node = model.findNode("NonExistent");
    return node == nullptr;
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
