// BehaviorTreePlugin.h - Behavior Tree Editor Plugin using UGF multi-component architecture
//
// Components:
//   BTModel           - Data: tree structure, selection state, CRUD operations
//   BTView            - ImGui UI: renders tree hierarchy with color-coded node types,
//                       context menu (add/delete/rename), drag-drop reparenting,
//                       properties panel, debug mode indicators
//   BTBehaviacBridge  - Behaviac integration: initializes workspace
//
// Plugin = ugf::IPlugin + ugf::ComponentSystem with multiple ugf::Component instances
#ifndef BEHAVIOR_TREE_PLUGIN_H
#define BEHAVIOR_TREE_PLUGIN_H
#include "imgui.h"

#include "PluginSystem.hpp"
#include "ComponentSystem.hpp"
#include "EventSystem.hpp"
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <unordered_map>
#include <any>
#include <algorithm>

/// Node type enumeration for behavior tree nodes
enum class BTNodeType { Selector, Sequence, Condition, Action };

/// BTNode - A single node in the behavior tree
struct BTNode : public std::enable_shared_from_this<BTNode> {
    BTNodeType type;
    std::string name;
    std::vector<std::shared_ptr<BTNode>> children;
    std::weak_ptr<BTNode> parent;

    // Extra node properties (persisted with the tree)
    std::string conditionScript;   // For Condition nodes: script expression
    std::string actionName;        // For Action nodes: action identifier

    // Debug state: true when this node is currently executing
    bool debugExecuting = false;

    BTNode(BTNodeType t, std::string n) : type(t), name(std::move(n)) {}

    void addChild(std::shared_ptr<BTNode> child) {
        child->parent = weak_from_this();
        children.push_back(std::move(child));
    }

    /// Remove a specific child from this node (shared_ptr ensures cleanup)
    bool removeChild(const std::shared_ptr<BTNode>& child) {
        auto it = std::find(children.begin(), children.end(), child);
        if (it == children.end()) return false;
        children.erase(it);
        return true;
    }

    /// Number of direct children
    size_t childCount() const { return children.size(); }

    /// Check whether this node is a descendant of potentialAncestor (for circular drag-drop rejection)
    bool isDescendantOf(const std::shared_ptr<BTNode>& potentialAncestor) const {
        auto p = parent.lock();
        while (p) {
            if (p == potentialAncestor) return true;
            p = p->parent.lock();
        }
        return false;
    }

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

    /// Add a node to a parent (creates the node and attaches it).
    /// Returns nullptr if parent is null.
    std::shared_ptr<BTNode> addNode(const std::string& name, BTNodeType type,
                                    std::shared_ptr<BTNode> parent) {
        if (!parent) return nullptr;
        auto node = std::make_shared<BTNode>(type, name);
        parent->addChild(node);
        return node;
    }

    /// Remove a node from the tree (detaches from parent; shared_ptr handles cleanup of children).
    /// Returns false if node is nullptr or is the root.
    bool removeNode(std::shared_ptr<BTNode> node) {
        if (!node || node == root_) return false;
        auto p = node->parent.lock();
        if (!p) return false;
        if (selected_ == node) selected_.reset();
        return p->removeChild(node);
    }

    /// Find a node by name (depth-first search). Returns nullptr if not found.
    std::shared_ptr<BTNode> findNode(const std::string& name) {
        if (!root_) return nullptr;
        return findNodeImpl(root_, name);
    }

    /// Generate a unique node name for a given type (e.g., "Selector_3")
    std::string generateName(BTNodeType type) const {
        const char* prefix = nullptr;
        switch (type) {
            case BTNodeType::Selector:  prefix = "Selector";  break;
            case BTNodeType::Sequence:  prefix = "Sequence";  break;
            case BTNodeType::Condition: prefix = "Condition"; break;
            case BTNodeType::Action:    prefix = "Action";    break;
        }
        if (!prefix) return "Unknown";
        int count = 0;
        traverse([&](const BTNode& node, int) {
            if (node.type == type) ++count;
        });
        return std::string(prefix) + "_" + std::to_string(count + 1);
    }

    /// Factory helper — creates a standalone node (no parent)
    static std::shared_ptr<BTNode> createNode(BTNodeType type, const std::string& name) {
        return std::make_shared<BTNode>(type, name);
    }

    /// Set root node (replaces existing tree)
    void setRoot(std::shared_ptr<BTNode> root) { root_ = std::move(root); }

    /// Selection management
    std::shared_ptr<BTNode> getSelected() const { return selected_; }
    void setSelected(std::shared_ptr<BTNode> node) { selected_ = std::move(node); }

    /// Traverse the tree in pre-order, calling visitor for each (node, depth)
    template<typename F>
    void traverse(F&& visitor, int depth = 0) const {
        if (!root_) return;
        // visitor is an lvalue (named parameter), pass by lvalue-ref for recursive traversal
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
        auto hasTarget = addNode("HasTarget", BTNodeType::Condition, attack);
        hasTarget->conditionScript = "target != null";
        auto doAttack = addNode("DoAttack", BTNodeType::Action, attack);
        doAttack->actionName = "AttackEnemy";
        auto idle = addNode("Idle", BTNodeType::Action, root_);
        idle->actionName = "PlayIdle";
    }

    template<typename F>
    void traverseImpl(const BTNode& node, F& visitor, int depth) const {
        visitor(node, depth);
        for (auto& child : node.children) {
            traverseImpl(*child, visitor, depth + 1);
        }
    }

    std::shared_ptr<BTNode> findNodeImpl(const std::shared_ptr<BTNode>& node,
                                         const std::string& name) const {
        if (node->name == name) return node;
        for (auto& child : node->children) {
            auto found = findNodeImpl(child, name);
            if (found) return found;
        }
        return nullptr;
    }

    std::shared_ptr<BTNode> root_;
    std::shared_ptr<BTNode> selected_;
};

// ============================================================
//  BTView : ugf::Component — ImGui tree visualization
//  Features: context menu, drag-drop reparenting, properties
//            panel, debug mode indicators
// ============================================================
class BTView : public ugf::Component {
public:
    std::string getComponentId() const override { return "BTView"; }

    bool initialize(const std::unordered_map<std::string, std::any>& config = {}) override;
    void update(float /*deltaTime*/) override;
    void terminate() override;

private:
    // --- Rendering ---
    void renderWindow();
    void renderToolbar();
    void renderTreePanel();
    void renderNode(const std::shared_ptr<BTNode>& node);
    void renderPropertiesPanel();
    ImVec4 colorForType(BTNodeType type) const;

    // --- Context menu ---
    void showNodeContextMenu(const std::shared_ptr<BTNode>& node);
    void showRenameModal();

    // --- Drag & drop ---
    void beginDragSource(const std::shared_ptr<BTNode>& node);
    void handleDropTarget(const std::shared_ptr<BTNode>& node);
    bool isValidDropTarget(const std::shared_ptr<BTNode>& source,
                           const std::shared_ptr<BTNode>& target) const;
    void reparentNode(std::shared_ptr<BTNode> source, std::shared_ptr<BTNode> newParent);

    // --- Debug ---
    void onPlayStateChanged(const struct PlayStateChangedEvent& event);
    void updateDebugSimulation();

    // Config / dependencies
    BTModel* model_ = nullptr;

    // Context menu state
    std::shared_ptr<BTNode> contextNode_;
    bool showRenamePopup_ = false;
    char renameBuffer_[128];

    // Properties editing buffers
    char propNameBuffer_[128];
    char condScriptBuffer_[256];
    char actionNameBuffer_[256];
    bool propDirty_ = false;

    // Drag & drop state
    std::shared_ptr<BTNode> dragSource_;
    int dragSourceId_ = 0;

    // Debug state
    bool debugMode_ = false;
    float debugTimer_ = 0.0f;
    int debugStep_ = 0;
    ugf::EventConnection playStateConn_;
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

    bool isWorkspaceReady() const { return workspaceReady_; }

private:
    bool workspaceReady_ = false;
};

// ============================================================
//  BehaviorTreePlugin : ugf::IPlugin — multi-component plugin
// ============================================================
class BehaviorTreePlugin : public ugf::IPlugin {
public:
    std::string getId() const override { return "BehaviorTree"; }
    bool initialize() override;
    void update(float dt) override;
    void shutdown() override;
};

#endif // BEHAVIOR_TREE_PLUGIN_H
