// SceneTreePlugin.h - 场景层级树插件（多组件架构）
// SceneTreeModel : ugf::Component  数据层 — 树节点层级
// SceneTreeView : ugf::Component   UI 层 — ImGui 渲染
// SceneTreeController : ugf::Component  逻辑层 — 选择/事件
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
//  SceneTreeModel : ugf::Component — 数据层
// ============================================================
class SceneTreeModel : public ugf::Component {
public:
    SceneTreeModel();
    ~SceneTreeModel() override;

    // ugf::Component 生命周期
    std::string getComponentId() const override;
    bool initialize(const std::unordered_map<std::string, std::any>& config) override;
    void update(float deltaTime) override;
    void terminate() override;

    // 树操作 API
    TreeNode* getRoot() const { return root_; }
    TreeNode* addNode(const std::string& name, TreeNode* parent);
    bool removeNode(const std::string& name);
    TreeNode* getSelectedNode() const { return selectedNode_; }
    void selectNode(const std::string& name);
    TreeNode* findNode(const std::string& name) const;
    int getNodeCount() const { return nodeCount_; }

private:
    TreeNode* root_ = nullptr;
    TreeNode* selectedNode_ = nullptr;
    int nodeCount_ = 1;

    TreeNode* findNodeRecursive(TreeNode* node, const std::string& name) const;
    bool removeNodeRecursive(TreeNode* parent, const std::string& name);
    void clearSelection(TreeNode* node);
};

// ============================================================
//  SceneTreeController : ugf::Component — 逻辑层（前置声明）
// ============================================================
class SceneTreeController : public ugf::Component {
public:
    SceneTreeController() = default;
    ~SceneTreeController() override = default;

    // ugf::Component 生命周期
    std::string getComponentId() const override;
    bool initialize(const std::unordered_map<std::string, std::any>& config) override;
    void update(float deltaTime) override;
    void terminate() override;

    // 选择处理 — 由 View 在点击时调用
    void onNodeClicked(const std::string& name);

private:
    SceneTreeModel* model_ = nullptr;
};

// ============================================================
//  SceneTreeView : ugf::Component — UI 层（ImGui）
// ============================================================
class SceneTreeView : public ugf::Component {
public:
    SceneTreeView() = default;
    ~SceneTreeView() override = default;

    // ugf::Component 生命周期
    std::string getComponentId() const override;
    bool initialize(const std::unordered_map<std::string, std::any>& config) override;
    void update(float deltaTime) override;
    void terminate() override;

private:
    void renderNode(TreeNode* node);

    SceneTreeModel* model_ = nullptr;
    SceneTreeController* controller_ = nullptr;
    bool windowOpen_ = true;
};

// ============================================================
//  SceneTreePlugin - 场景树插件 (ugf::IPlugin)
// ============================================================
class SceneTreePlugin : public ugf::IPlugin {
public:
    SceneTreePlugin() = default;
    ~SceneTreePlugin() override = default;

    std::string getId() const override;
    bool initialize() override;
    void update(float deltaTime) override;
    void shutdown() override;
};

#endif // SCENE_TREE_PLUGIN_H
