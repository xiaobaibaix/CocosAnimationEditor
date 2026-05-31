// SceneViewPlugin.cpp - 场景视图插件实现
// 渲染可视化场景节点、支持视口平移/缩放、动画帧预览
#include "plugins/scene_view/SceneViewPlugin.h"
#include "editor/EditorEvents.h"
#include "EventSystem.hpp"
#include "imgui_internal.h"
#include "cocos2d.h"

// ============================================================
//  SceneViewModel
// ============================================================

bool SceneViewModel::initialize(const std::unordered_map<std::string, std::any>&) {
    // Subscribe to scene tree events -- sync visual nodes
    ugf::EventBus::getInstance().subscribe<NodeAddedEvent>(
        [this](const NodeAddedEvent& e) {
            ImU32 colors[] = {IM_COL32(255,140,60,255),IM_COL32(60,180,80,255),
                IM_COL32(100,160,240,255),IM_COL32(240,200,100,255),IM_COL32(200,120,200,255)};
            addNode(e.nodeName, ImVec2(300+nodes_.size()*30, 350+nodes_.size()*20),
                    ImVec2(50,50), colors[nodes_.size()%5]);
        });
    ugf::EventBus::getInstance().subscribe<NodeRemovedEvent>(
        [this](const NodeRemovedEvent& e) { removeNode(e.nodeName); });
    ugf::EventBus::getInstance().subscribe<NodeRenamedEvent>(
        [this](const NodeRenamedEvent& e) {
            auto* node = findNode(e.oldName);
            if (node) node->name = e.newName;
        });
    ugf::EventBus::getInstance().subscribe<NodeSelectedEvent>(
        [this](const NodeSelectedEvent& e) { setSelectedNode(e.nodeName); });

    addNode("Character",  ImVec2(300, 400), ImVec2(60, 80),  IM_COL32(255, 140, 60, 255));
    addNode("Ground",     ImVec2(400, 520), ImVec2(500, 20), IM_COL32(100, 180, 100, 255));
    addNode("Sky",        ImVec2(400, 100), ImVec2(500, 80), IM_COL32(100, 160, 240, 200));
    addNode("Tree_01",    ImVec2(150, 420), ImVec2(40, 60),  IM_COL32(60, 180, 80, 255));
    addNode("Tree_02",    ImVec2(600, 430), ImVec2(35, 55),  IM_COL32(60, 180, 80, 255));
    addNode("Bone_Arm",   ImVec2(280, 370), ImVec2(20, 40),  IM_COL32(240, 200, 100, 255));
    addNode("Bone_Leg",   ImVec2(310, 440), ImVec2(22, 50),  IM_COL32(240, 200, 100, 255));
    addNode("Sprite_Hero",ImVec2(300, 380), ImVec2(50, 60),  IM_COL32(255, 220, 120, 255));
    CCLOG("[SceneViewModel] Initialized with %zu nodes", nodes_.size());
    return true;
}

void SceneViewModel::update(float) {}
void SceneViewModel::terminate() { nodes_.clear(); }

void SceneViewModel::addNode(const std::string& name, ImVec2 pos, ImVec2 size, ImU32 color) {
    VisualNode node;
    node.name = name;
    node.position = pos;
    node.size = size;
    node.color = color;
    nodes_.push_back(node);
}

void SceneViewModel::removeNode(const std::string& name) {
    nodes_.erase(std::remove_if(nodes_.begin(), nodes_.end(),
        [&](const VisualNode& n) { return n.name == name; }), nodes_.end());
}

void SceneViewModel::setNodePosition(const std::string& name, ImVec2 pos) {
    auto* node = findNode(name);
    if (node) node->position = pos;
}

VisualNode* SceneViewModel::findNode(const std::string& name) {
    for (auto& n : nodes_) {
        if (n.name == name) return &n;
    }
    return nullptr;
}

void SceneViewModel::setSelectedNode(const std::string& name) {
    selectedNodeName_ = name;
}

// ============================================================
//  SceneViewRenderer
// ============================================================

bool SceneViewRenderer::initialize(const std::unordered_map<std::string, std::any>& config) {
    auto it = config.find("model");
    if (it != config.end()) {
        model_ = std::any_cast<SceneViewModel*>(it->second);
    }
    CCLOG("[SceneViewRenderer] Initialized (model=%p)", static_cast<void*>(model_));
    return true;
}

void SceneViewRenderer::update(float) {
    if (!windowOpen_) return;
    if (!ImGui::Begin("Scene View", &windowOpen_)) {
        ImGui::End();
        return;
    }
    renderCanvas();
    ImGui::End();
}

void SceneViewRenderer::terminate() { model_ = nullptr; }

void SceneViewRenderer::renderCanvas() {
    ImVec2 canvasPos = ImGui::GetCursorScreenPos();
    ImVec2 availSize = ImGui::GetContentRegionAvail();
    if (availSize.x < 50 || availSize.y < 50) return;
    canvasSize_ = availSize;

    ImDrawList* dl = ImGui::GetWindowDrawList();

    // 画布背景
    dl->AddRectFilled(canvasPos,
        ImVec2(canvasPos.x + canvasSize_.x, canvasPos.y + canvasSize_.y),
        IM_COL32(30, 30, 35, 255));

    // 裁剪区域
    dl->PushClipRect(canvasPos,
        ImVec2(canvasPos.x + canvasSize_.x, canvasPos.y + canvasSize_.y), true);

    // 网格
    renderGrid();

    // 场景节点
    if (model_) {
        for (const auto& node : model_->getNodes()) {
            renderNode(node);
        }
        auto selName = model_->getSelectedNode();
        if (!selName.empty()) {
            auto* selNode = model_->findNode(selName);
            if (selNode) renderSelectionOutline(*selNode);
        }
    }

    dl->PopClipRect();

    // --- 鼠标交互 ---
    ImGui::InvisibleButton("##SceneCanvas", canvasSize_,
        ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonMiddle |
        ImGuiButtonFlags_MouseButtonRight);

    bool hovered = ImGui::IsItemHovered();
    ImVec2 mousePos = ImGui::GetMousePos();

    // 缩放（滚轮）
    if (hovered) {
        float wheel = ImGui::GetIO().MouseWheel;
        if (wheel != 0.0f) {
            zoom_ *= (1.0f + wheel * 0.1f);
            zoom_ = std::max(0.1f, std::min(zoom_, 10.0f));
        }
    }

    // 平移（中键 / Ctrl+右键拖拽）
    if (ImGui::IsItemClicked(ImGuiMouseButton_Middle) ||
        (ImGui::IsItemClicked(ImGuiMouseButton_Right) && ImGui::GetIO().KeyCtrl)) {
        panning_ = true;
        panStartMouse_ = mousePos;
        panStartOffset_ = panOffset_;
    }
    if (panning_ && (ImGui::IsMouseDragging(ImGuiMouseButton_Middle) ||
                      ImGui::IsMouseDragging(ImGuiMouseButton_Right))) {
        ImVec2 delta = ImVec2(mousePos.x - panStartMouse_.x,
                               mousePos.y - panStartMouse_.y);
        panOffset_ = ImVec2(panStartOffset_.x + delta.x,
                             panStartOffset_.y + delta.y);
    }
    if (panning_ && ImGui::IsMouseReleased(ImGuiMouseButton_Middle)) panning_ = false;
    if (panning_ && ImGui::IsMouseReleased(ImGuiMouseButton_Right)) panning_ = false;

    // 左键点击选节点
    if (hovered && ImGui::IsItemClicked(ImGuiMouseButton_Left) && model_) {
        ImVec2 worldPos = screenToWorld(mousePos);
        const auto& nodes = model_->getNodes();
        for (auto it = nodes.rbegin(); it != nodes.rend(); ++it) {
            ImVec2 nMin = it->position;
            ImVec2 nMax = ImVec2(nMin.x + it->size.x, nMin.y + it->size.y);
            if (worldPos.x >= nMin.x && worldPos.x <= nMax.x &&
                worldPos.y >= nMin.y && worldPos.y <= nMax.y) {
                model_->setSelectedNode(it->name);
                ugf::EventBus::getInstance().publish(NodeSelectedEvent{it->name});
                CCLOG("[SceneView] Selected: %s", it->name.c_str());
                break;
            }
        }
    }

    // 视口信息
    ImVec2 infoPos = ImVec2(canvasPos.x + 8, canvasPos.y + canvasSize_.y - 20);
    char infoBuf[128];
    snprintf(infoBuf, sizeof(infoBuf), "Zoom: %.0f%%  |  Pan: (%.0f, %.0f)",
             zoom_ * 100.0f, panOffset_.x, panOffset_.y);
    dl->AddText(infoPos, IM_COL32(150, 150, 150, 255), infoBuf);
}

void SceneViewRenderer::renderGrid() {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 canvasPos = ImGui::GetCursorScreenPos();

    ImVec2 worldMin = screenToWorld(canvasPos);
    ImVec2 worldMax = screenToWorld(ImVec2(canvasPos.x + canvasSize_.x,
                                            canvasPos.y + canvasSize_.y));
    ImU32 gridCol = IM_COL32(60, 60, 65, 255);
    ImU32 axisCol = IM_COL32(80, 80, 85, 255);

    float step = 50.0f;
    for (float x = std::floor(worldMin.x / step) * step; x <= worldMax.x; x += step) {
        ImVec2 a = worldToScreen(ImVec2(x, worldMin.y));
        ImVec2 b = worldToScreen(ImVec2(x, worldMax.y));
        dl->AddLine(a, b, (static_cast<int>(x) == 0) ? axisCol : gridCol, 1.0f);
    }
    for (float y = std::floor(worldMin.y / step) * step; y <= worldMax.y; y += step) {
        ImVec2 a = worldToScreen(ImVec2(worldMin.x, y));
        ImVec2 b = worldToScreen(ImVec2(worldMax.x, y));
        dl->AddLine(a, b, (static_cast<int>(y) == 0) ? axisCol : gridCol, 1.0f);
    }
}

void SceneViewRenderer::renderNode(const VisualNode& node) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 sMin = worldToScreen(node.position);
    ImVec2 sMax = worldToScreen(ImVec2(node.position.x + node.size.x,
                                        node.position.y + node.size.y));

    // 可视裁剪
    ImVec2 cp = ImGui::GetCursorScreenPos();
    if (sMax.x < cp.x || sMin.x > cp.x + canvasSize_.x ||
        sMax.y < cp.y || sMin.y > cp.y + canvasSize_.y) return;

    float r = std::min(node.size.x, node.size.y) * zoom_ * 0.15f;
    if (r < 1.0f) r = 1.0f;
    if (r > 8.0f) r = 8.0f;

    dl->AddRectFilled(sMin, sMax, node.color, r);
    dl->AddRect(sMin, sMax, IM_COL32(255, 255, 255, 80), r);

    if (node.size.x * zoom_ > 30.0f && node.size.y * zoom_ > 15.0f) {
        ImVec2 ts = ImGui::CalcTextSize(node.name.c_str());
        ImVec2 ct = ImVec2((sMin.x + sMax.x) * 0.5f, (sMin.y + sMax.y) * 0.5f);
        dl->AddText(ImVec2(ct.x - ts.x * 0.5f, ct.y - ts.y * 0.5f),
            IM_COL32(255, 255, 255, 220), node.name.c_str());
    }
}

void SceneViewRenderer::renderSelectionOutline(const VisualNode& node) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 sMin = worldToScreen(node.position);
    ImVec2 sMax = worldToScreen(ImVec2(node.position.x + node.size.x,
                                        node.position.y + node.size.y));
    float o = 3.0f;
    dl->AddRect(ImVec2(sMin.x - o, sMin.y - o), ImVec2(sMax.x + o, sMax.y + o),
        IM_COL32(255, 220, 80, 255), 0.0f, 0, 2.5f);

    // 四角标记
    float cs = 6.0f;
    ImU32 cc = IM_COL32(255, 220, 80, 255);
    auto corner = [&](float x, float y) {
        dl->AddRectFilled(ImVec2(x - cs, y - cs), ImVec2(x + cs, y + cs), cc);
    };
    corner(sMin.x - o, sMin.y - o);
    corner(sMax.x + o, sMin.y - o);
    corner(sMin.x - o, sMax.y + o);
    corner(sMax.x + o, sMax.y + o);
}

ImVec2 SceneViewRenderer::worldToScreen(ImVec2 wp) const {
    ImVec2 cp = ImGui::GetCursorScreenPos();
    float cx = cp.x + canvasSize_.x * 0.5f;
    float cy = cp.y + canvasSize_.y * 0.5f;
    return ImVec2(cx + (wp.x - 400 + panOffset_.x) * zoom_,
                   cy + (wp.y - 300 + panOffset_.y) * zoom_);
}

ImVec2 SceneViewRenderer::screenToWorld(ImVec2 sp) const {
    ImVec2 cp = ImGui::GetCursorScreenPos();
    float cx = cp.x + canvasSize_.x * 0.5f;
    float cy = cp.y + canvasSize_.y * 0.5f;
    return ImVec2((sp.x - cx) / zoom_ + 400 - panOffset_.x,
                   (sp.y - cy) / zoom_ + 300 - panOffset_.y);
}

// ============================================================
//  SceneViewPlugin
// ============================================================

bool SceneViewPlugin::initialize() {
    CCLOG("[SceneViewPlugin] Initializing...");
    auto* model = componentSystem.registerComponent<SceneViewModel>("model");
    if (!model) { CCLOG("[SceneViewPlugin] Failed to register SceneViewModel"); return false; }
    std::unordered_map<std::string, std::any> cfg;
    cfg["model"] = model;
    auto* renderer = componentSystem.registerComponent<SceneViewRenderer>("renderer", cfg);
    if (!renderer) { CCLOG("[SceneViewPlugin] Failed to register SceneViewRenderer"); return false; }
    CCLOG("[SceneViewPlugin] Initialized — %zu nodes", model->getNodes().size());
    return true;
}

void SceneViewPlugin::update(float dt) { componentSystem.updateAll(dt); }
void SceneViewPlugin::shutdown() { componentSystem.clear(); }
