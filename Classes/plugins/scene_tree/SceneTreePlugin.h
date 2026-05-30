// SceneTreePlugin.h - 场景层级树插件
// 显示场景节点层级结构，支持点击选中
// 实现 ugf::IPlugin，由 EditorApp 的 ugf::PluginSystem 管理
#ifndef SCENE_TREE_PLUGIN_H
#define SCENE_TREE_PLUGIN_H

#include "PluginSystem.hpp"
#include <string>
#include <vector>

// ============================================================
//  TreeNode - 场景树节点
// ============================================================
struct TreeNode {
    std::string name;
    std::vector<TreeNode*> children;
    TreeNode* parent = nullptr;
    bool selected = false;

    explicit TreeNode(const std::string& n) : name(n) {}
    ~TreeNode();
};

// ============================================================
//  SceneTreeModel - 场景树数据模型
// ============================================================
class SceneTreeModel {
public:
    SceneTreeModel();
    ~SceneTreeModel();

    /// 获取根节点
    TreeNode* getRoot() const { return root_; }

    /// 添加子节点到指定父节点
    TreeNode* addNode(const std::string& name, TreeNode* parent);

    /// 移除节点（递归删除子节点）
    bool removeNode(const std::string& name);

    /// 获取当前选中的节点
    TreeNode* getSelectedNode() const { return selectedNode_; }

    /// 选中指定名称的节点
    void selectNode(const std::string& name);

    /// 按名称查找节点
    TreeNode* findNode(const std::string& name) const;

    /// 获取总节点数
    int getNodeCount() const { return nodeCount_; }

private:
    TreeNode* root_;
    TreeNode* selectedNode_ = nullptr;
    int nodeCount_ = 1; // includes root

    TreeNode* findNodeRecursive(TreeNode* node, const std::string& name) const;
    bool removeNodeRecursive(TreeNode* parent, const std::string& name);
    void clearSelection(TreeNode* node);
};

// ============================================================
//  SceneTreePlugin - 场景树插件 (ugf::IPlugin)
// ============================================================
class SceneTreePlugin : public ugf::IPlugin {
public:
    SceneTreePlugin();
    ~SceneTreePlugin() override;

    // ugf::IPlugin interface
    std::string getId() const override;
    bool initialize() override;
    void update(float deltaTime) override;
    void shutdown() override;

private:
    void renderTreeNode(TreeNode* node);
    SceneTreeModel model_;
    bool windowOpen_ = true;
};

#endif // SCENE_TREE_PLUGIN_H
