// SceneViewPlugin.h - 场景视图插件（UGF 多组件架构）
// SceneViewModel    : ugf::Component — 场景节点数据 + 视口状态
// SceneViewRenderer : ugf::Component — ImGui 画布渲染（节点可视化 + 动画预览）
// SceneViewPlugin   : ugf::IPlugin     — 插件入口
#ifndef SCENE_VIEW_PLUGIN_H
#define SCENE_VIEW_PLUGIN_H

#include "PluginSystem.hpp"
#include "ComponentSystem.hpp"
#include "imgui.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <any>

// ============================================================
//  VisualNode — 场景中可视化节点的数据
// ============================================================
struct VisualNode {
    std::string name;
    ImVec2 position;
    ImVec2 size;
    ImU32 color;
    bool visible = true;
};

// ============================================================
//  SceneViewModel : ugf::Component — 数据层
// ============================================================
class SceneViewModel : public ugf::Component {
public:
    std::string getComponentId() const override { return "SceneView.Model"; }
    bool initialize(const std::unordered_map<std::string, std::any>& config = {}) override;
    void update(float deltaTime) override;
    void terminate() override;

    void addNode(const std::string& name, ImVec2 pos, ImVec2 size, ImU32 color);
    void removeNode(const std::string& name);
    void setNodePosition(const std::string& name, ImVec2 pos);
    VisualNode* findNode(const std::string& name);
    const std::vector<VisualNode>& getNodes() const { return nodes_; }

    void setSelectedNode(const std::string& name);
    std::string getSelectedNode() const { return selectedNodeName_; }

private:
    std::vector<VisualNode> nodes_;
    std::string selectedNodeName_;
};

// ============================================================
//  SceneViewRenderer : ugf::Component — 视图层
// ============================================================
class SceneViewRenderer : public ugf::Component {
public:
    std::string getComponentId() const override { return "SceneView.Renderer"; }
    bool initialize(const std::unordered_map<std::string, std::any>& config = {}) override;
    void update(float deltaTime) override;
    void terminate() override;

private:
    void renderCanvas();
    void renderGrid();
    void renderNode(const VisualNode& node);
    void renderSelectionOutline(const VisualNode& node);
    ImVec2 worldToScreen(ImVec2 worldPos) const;
    ImVec2 screenToWorld(ImVec2 screenPos) const;

    SceneViewModel* model_ = nullptr;
    bool windowOpen_ = true;

    ImVec2 panOffset_ = ImVec2(0, 0);
    float zoom_ = 1.0f;
    ImVec2 canvasSize_ = ImVec2(800, 600);

    bool panning_ = false;
    ImVec2 panStartMouse_;
    ImVec2 panStartOffset_;
};

// ============================================================
//  SceneViewPlugin — 插件入口
// ============================================================
class SceneViewPlugin : public ugf::IPlugin {
public:
    std::string getId() const override { return "SceneView"; }
    bool initialize() override;
    void update(float deltaTime) override;
    void shutdown() override;
};

#endif // SCENE_VIEW_PLUGIN_H
