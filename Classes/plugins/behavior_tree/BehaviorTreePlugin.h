// BehaviorTreePlugin.h - Behavior Tree Editor Plugin using UGF multi-component architecture
//
// Components:
//   BTModel           - Data: tree structure, selection state
//   BTView            - ImGui UI: renders tree hierarchy with color-coded node types
//   BTBehaviacBridge  - Behaviac integration: initializes workspace
//
// Plugin = ugf::IPlugin + ugf::ComponentSystem with multiple ugf::Component instances
#ifndef BEHAVIOR_TREE_PLUGIN_H
#define BEHAVIOR_TREE_PLUGIN_H
#include "imgui.h"

#include "PluginSystem.hpp"
#include "ComponentSystem.hpp"
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <unordered_map>
#include <any>

/// Node type enumeration for behavior tree nodes
enum class BTNodeType { Selector, Sequence, Condition, Action };

/// BTNode - A single node in the behavior tree
struct BTNode : public std::enable_shared_from_this<BTNode> {
    BTNodeType type;
    std::string name;
    std::vector<std::shared_ptr<BTNode>> children;
    std::weak_ptr<BTNode> parent;

    BTNode(BTNodeType t, std::string n) : type(t), name(std::move(n)) {}

    void addChild(std::shared_ptr<BTNode> child) {
        child->parent = weak_from_this();
        children.push_back(std::move(child));
    }

    /// Number of direct children
    size_t childCount() const { return children.size(); }

    const char* typeName() const {
        switch (type) {
            case BTNodeType::Selector:  return "Selector";
            case BTNodeType::Sequence:  return "Sequence";
            case BTNodeType::Condition: return "Condition";
            case BTNodeType::Action:    return "Action";
        }
        return "Unknown";
    }
};

// ============================================================
//  BTModel : ugf::Component — behavior tree data model
// ============================================================
class BTModel : public ugf::Component {
public:
    std::string getComponentId() const override { return "BTModel"; }

    bool initialize(const std::unordered_map<std::string, std::any>& config = {}) override {
        buildDefaultTree();
        return true;
    }

    void update(float /*deltaTime*/) override {}
    void terminate() override { root_.reset(); selected_.reset(); }

    /// Get the root node
    std::shared_ptr<BTNode> getRoot() const { return root_; }

    /// Add a node to a parent (creates the node and attaches it)
    std::shared_ptr<BTNode> addNode(const std::string& name, BTNodeType type,
                                    std::shared_ptr<BTNode> parent) {
        auto node = std::make_shared<BTNode>(type, name);
        if (parent) {
            parent->addChild(node);
        }
        return node;
    }

    /// Factory helper — creates a standalone node (no parent)
    static std::shared_ptr<BTNode> createNode(BTNodeType type, const std::string& name) {
        return std::make_shared<BTNode>(type, name);
    }

    /// Set root node (for tests, replaces tree)
    void setRoot(std::shared_ptr<BTNode> root) { root_ = std::move(root); }

    /// Selection management
    std::shared_ptr<BTNode> getSelected() const { return selected_; }
    void setSelected(std::shared_ptr<BTNode> node) { selected_ = std::move(node); }

    /// Traverse the tree in pre-order, calling visitor for each (node, depth)
    void traverse(std::function<void(const BTNode&, int)> visitor, int depth = 0) const {
        if (!root_) return;
        traverseImpl(*root_, visitor, depth);
    }

    /// Count total nodes (including root)
    size_t nodeCount() const {
        size_t count = 0;
        traverse([&count](const BTNode&, int) { ++count; });
        return count;
    }

private:
    void buildDefaultTree() {
        // Default tree:
        //   Root (Selector)
        //   |-- Attack (Sequence)
        //   |   |-- HasTarget (Condition)
        //   |   |-- DoAttack (Action)
        //   |-- Idle (Action)
        root_ = std::make_shared<BTNode>(BTNodeType::Selector, "Root");
        auto attack = addNode("Attack", BTNodeType::Sequence, root_);
        addNode("HasTarget", BTNodeType::Condition, attack);
        addNode("DoAttack", BTNodeType::Action, attack);
        addNode("Idle", BTNodeType::Action, root_);
    }

    void traverseImpl(const BTNode& node,
                      std::function<void(const BTNode&, int)>& visitor,
                      int depth) const {
        visitor(node, depth);
        for (auto& child : node.children) {
            traverseImpl(*child, visitor, depth + 1);
        }
    }

    std::shared_ptr<BTNode> root_;
    std::shared_ptr<BTNode> selected_;
};

// ============================================================
//  BTView : ugf::Component — ImGui tree visualization
// ============================================================
class BTView : public ugf::Component {
public:
    std::string getComponentId() const override { return "BTView"; }

    bool initialize(const std::unordered_map<std::string, std::any>& config = {}) override {
        if (config.count("model")) {
            try {
                model_ = std::any_cast<BTModel*>(config.at("model"));
            } catch (const std::bad_any_cast&) {
                return false;
            }
        }
        return model_ != nullptr;
    }

    void update(float /*deltaTime*/) override;
    void terminate() override {}

private:
    void renderWindow();
    void renderNode(const std::shared_ptr<BTNode>& node);
    ImVec4 colorForType(BTNodeType type) const;

    BTModel* model_ = nullptr;
};

// ============================================================
//  BTBehaviacBridge : ugf::Component — Behaviac integration
// ============================================================
class BTBehaviacBridge : public ugf::Component {
public:
    std::string getComponentId() const override { return "BTBehaviacBridge"; }

    bool initialize(const std::unordered_map<std::string, std::any>& /*config*/ = {}) override;
    void update(float /*deltaTime*/) override {}
    void terminate() override {}
};

// ============================================================
//  BehaviorTreePlugin : ugf::IPlugin — multi-component plugin
// ============================================================
class BehaviorTreePlugin : public ugf::IPlugin {
public:
    std::string getId() const override { return "BehaviorTree"; }
    bool initialize() override;
    void shutdown() override;
};

#endif // BEHAVIOR_TREE_PLUGIN_H
