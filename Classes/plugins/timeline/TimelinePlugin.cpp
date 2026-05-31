// TimelinePlugin.cpp - 时间轴插件实现（多组件架构）
#include "TimelinePlugin.h"
#include "imgui.h"
#include "cocos2d.h"
#include "EventSystem.hpp"
#include "tests/TestFramework.h"
#include <algorithm>
#include <cstring>
#include <cmath>

// ================================================================
//  TimelineData 实现
// ================================================================

bool TimelineData::initialize(const std::unordered_map<std::string, std::any>&) {
    totalFrames = 60;
    currentFrame = 0;
    isPlaying = false;
    fps = 30;
    CCLOG("[TimelineData] initialized (totalFrames=%d, fps=%d)", totalFrames, fps);
    return true;
}

void TimelineData::update(float) {
    // 纯数据模型，无每帧逻辑
}

void TimelineData::terminate() {
    CCLOG("[TimelineData] terminated");
}

void TimelineData::play() {
    isPlaying = true;
}

void TimelineData::pause() {
    isPlaying = false;
}

void TimelineData::seekTo(int frame) {
    frame = clampFrame(frame);
    currentFrame = frame;
}

int TimelineData::clampFrame(int frame) const {
    if (frame < 0) return 0;
    if (frame > totalFrames) return totalFrames;
    return frame;
}

void TimelineData::advance() {
    currentFrame++;
    if (currentFrame > totalFrames) {
        currentFrame = 0;
    }
}

// --- 轨道管理 ---

void TimelineData::addTrack(const std::string& name, const std::string& targetNode,
                            const std::string& property) {
    Track track;
    track.name = name;
    track.targetNode = targetNode;
    track.property = property;
    tracks_.push_back(track);
    CCLOG("[TimelineData] Added track '%s' -> %s.%s", name.c_str(),
          targetNode.c_str(), property.c_str());
}

void TimelineData::removeTrack(int trackIndex) {
    if (trackIndex < 0 || trackIndex >= static_cast<int>(tracks_.size())) return;
    CCLOG("[TimelineData] Removed track '%s'", tracks_[trackIndex].name.c_str());
    tracks_.erase(tracks_.begin() + trackIndex);
}

// --- 关键帧管理 ---

void TimelineData::addKeyframe(int trackIndex, int frame, float value, int easeType) {
    if (trackIndex < 0 || trackIndex >= static_cast<int>(tracks_.size())) return;
    auto& kfs = tracks_[trackIndex].keyframes;
    frame = clampFrame(frame);

    // 插入到正确位置（按帧号排序）
    Keyframe kf;
    kf.frame = frame;
    kf.value = value;
    kf.easeType = easeType;

    auto it = kfs.begin();
    while (it != kfs.end() && it->frame < frame) ++it;
    kfs.insert(it, kf);

    CCLOG("[TimelineData] Added keyframe at frame %d (value=%.2f) to track '%s'",
          frame, value, tracks_[trackIndex].name.c_str());
}

void TimelineData::removeKeyframe(int trackIndex, int keyframeIndex) {
    if (trackIndex < 0 || trackIndex >= static_cast<int>(tracks_.size())) return;
    auto& kfs = tracks_[trackIndex].keyframes;
    if (keyframeIndex < 0 || keyframeIndex >= static_cast<int>(kfs.size())) return;

    CCLOG("[TimelineData] Removed keyframe %d from track '%s'",
          keyframeIndex, tracks_[trackIndex].name.c_str());
    kfs.erase(kfs.begin() + keyframeIndex);
}

void TimelineData::moveKeyframe(int trackIndex, int keyframeIndex,
                                 int newFrame, float newValue) {
    if (trackIndex < 0 || trackIndex >= static_cast<int>(tracks_.size())) return;
    auto& kfs = tracks_[trackIndex].keyframes;
    if (keyframeIndex < 0 || keyframeIndex >= static_cast<int>(kfs.size())) return;

    newFrame = clampFrame(newFrame);
    int easeType = kfs[keyframeIndex].easeType;
    kfs.erase(kfs.begin() + keyframeIndex);
    addKeyframe(trackIndex, newFrame, newValue, easeType);
}

void TimelineData::setKeyframeEasing(int trackIndex, int keyframeIndex, int easeType) {
    if (trackIndex < 0 || trackIndex >= static_cast<int>(tracks_.size())) return;
    auto& kfs = tracks_[trackIndex].keyframes;
    if (keyframeIndex < 0 || keyframeIndex >= static_cast<int>(kfs.size())) return;
    kfs[keyframeIndex].easeType = std::max(0, std::min(easeType, 3));
}

int TimelineData::getKeyframeCount() const {
    int total = 0;
    for (const auto& t : tracks_) total += static_cast<int>(t.keyframes.size());
    return total;
}

float TimelineData::interpolate(int trackIndex, int frame) const {
    if (trackIndex < 0 || trackIndex >= static_cast<int>(tracks_.size())) return 0.0f;
    const auto& kfs = tracks_[trackIndex].keyframes;
    if (kfs.empty()) return 0.0f;
    if (kfs.size() == 1) return kfs[0].value;

    // 边界外推
    if (frame <= kfs.front().frame) return kfs.front().value;
    if (frame >= kfs.back().frame) return kfs.back().value;

    // 查找所在区间
    for (size_t i = 0; i < kfs.size() - 1; ++i) {
        if (frame >= kfs[i].frame && frame <= kfs[i + 1].frame) {
            float range = static_cast<float>(kfs[i + 1].frame - kfs[i].frame);
            if (range <= 0.0f) return kfs[i].value;
            float t = static_cast<float>(frame - kfs[i].frame) / range;

            // 应用缓动函数
            switch (kfs[i].easeType) {
                case 0: /* Linear */ break;
                case 1: t = t * t; break;                         // EaseIn (Quad)
                case 2: t = t * (2.0f - t); break;                // EaseOut (Quad)
                case 3:                                          // EaseInOut (Quad)
                    t = t < 0.5f ? 2.0f * t * t
                                 : -1.0f + (4.0f - 2.0f * t) * t;
                    break;
            }

            return kfs[i].value + t * (kfs[i + 1].value - kfs[i].value);
        }
    }
    return kfs.back().value;
}

// ================================================================
//  TimelineView 实现
// ================================================================

// 缓动类型名称
static const char* kEaseNames[] = {"Linear", "Ease In", "Ease Out", "Ease InOut"};

// 缓动类型颜色
static ImU32 kEaseColors[] = {
    IM_COL32(200, 200, 200, 255),  // Linear - gray
    IM_COL32(100, 255, 100, 255),  // EaseIn - green
    IM_COL32(100, 100, 255, 255),  // EaseOut - blue
    IM_COL32(255, 150, 50, 255)    // EaseInOut - orange
};

bool TimelineView::initialize(const std::unordered_map<std::string, std::any>& config) {
    auto it = config.find("data");
    if (it == config.end()) {
        CCLOG("[TimelineView] missing 'data' in config");
        return false;
    }
    data_ = std::any_cast<TimelineData*>(it->second);
    if (!data_) {
        CCLOG("[TimelineView] null data pointer");
        return false;
    }
    CCLOG("[TimelineView] initialized");
    return true;
}

void TimelineView::update(float) {
    if (!data_ || !windowOpen_) return;
    renderTimelineWindow();
}

void TimelineView::terminate() {
    CCLOG("[TimelineView] terminated");
}

// --- 坐标转换 ---

float TimelineView::frameToX(float frame, float originX) const {
    return originX + kHeaderWidth + frame * pixelsPerFrame_ - scrollX_;
}

float TimelineView::xToFrame(float x, float originX) const {
    return (x - originX - kHeaderWidth + scrollX_) / pixelsPerFrame_;
}

bool TimelineView::hitTestKeyframe(float mouseX, float mouseY,
                                    float kfX, float lineY) const {
    float dx = mouseX - kfX;
    float dy = mouseY - lineY;
    return (dx * dx + dy * dy) <= (kKeyframeHitRadius * kKeyframeHitRadius);
}

// ================================================================
//  renderTimelineWindow — 主渲染入口
// ================================================================

void TimelineView::renderTimelineWindow() {
    ImGui::SetNextWindowSize(ImVec2(820, 450), ImGuiCond_FirstUseEver);
    ImGui::Begin("Timeline", &windowOpen_);

    renderPlaybackControls();
    ImGui::Spacing();

    float availWidth = ImGui::GetContentRegionAvail().x;

    // --- 缩放处理 (鼠标滚轮) ---
    ImVec2 windowPos = ImGui::GetWindowPos();
    ImVec2 contentMin = ImGui::GetWindowContentRegionMin();
    ImVec2 contentMax = ImGui::GetWindowContentRegionMax();
    ImVec2 mousePos = ImGui::GetMousePos();

    bool inContent =
        (mousePos.x >= windowPos.x + contentMin.x) &&
        (mousePos.x <= windowPos.x + contentMax.x) &&
        (mousePos.y >= windowPos.y + contentMin.y) &&
        (mousePos.y <= windowPos.y + contentMax.y);

    if (inContent) {
        float wheel = ImGui::GetIO().MouseWheel;
        if (wheel != 0.0f) {
            float oldPPF = pixelsPerFrame_;
            pixelsPerFrame_ *= (wheel > 0.0f) ? 1.15f : 0.87f;
            pixelsPerFrame_ = std::max(1.0f, std::min(pixelsPerFrame_, 50.0f));

            // 缩放时保持鼠标位置对应的帧不变
            float cursorX = mousePos.x - windowPos.x - contentMin.x;
            float frameAtCursor = xToFrame(windowPos.x + contentMin.x + cursorX,
                                           windowPos.x + contentMin.x);
            scrollX_ += frameAtCursor * (pixelsPerFrame_ - oldPPF);
            scrollX_ = std::max(0.0f, scrollX_);
        }

        // 中键拖拽平移
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
            scrollX_ -= ImGui::GetIO().MouseDelta.x;
            scrollX_ = std::max(0.0f, scrollX_);
        }
    }

    renderTimeRuler();
    renderTrackList();

    // --- 水平滚动条 ---
    float contentTotalWidth = data_->totalFrames * pixelsPerFrame_;
    float viewWidth = std::max(1.0f, availWidth - kHeaderWidth);
    float maxScroll = std::max(0.0f, contentTotalWidth - viewWidth);

    if (maxScroll > 0.0f) {
        float barHeight = 12.0f;
        ImGui::InvisibleButton("##HScrollBar", ImVec2(availWidth, barHeight));
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 barPos = ImGui::GetItemRectMin();

        dl->AddRectFilled(barPos,
                          ImVec2(barPos.x + availWidth, barPos.y + barHeight),
                          IM_COL32(50, 50, 55, 255));

        float thumbWidth = std::max(24.0f, viewWidth / contentTotalWidth * availWidth);
        float thumbX = barPos.x + (scrollX_ / maxScroll) * (availWidth - thumbWidth);
        dl->AddRectFilled(ImVec2(thumbX, barPos.y),
                          ImVec2(thumbX + thumbWidth, barPos.y + barHeight),
                          IM_COL32(90, 90, 100, 255));

        if (ImGui::IsItemHovered() || ImGui::IsItemActive()) {
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                float ratio = (ImGui::GetMousePos().x - barPos.x) / availWidth;
                scrollX_ = ratio * maxScroll;
            }
            if (ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
                scrollX_ += ImGui::GetIO().MouseDelta.x *
                           (maxScroll / (availWidth - thumbWidth));
                scrollX_ = std::max(0.0f, std::min(scrollX_, maxScroll));
            }
        }
    } else {
        scrollX_ = 0.0f;
    }

    // --- 弹出窗口 ---
    // 关键帧右键菜单
    if (pendingKeyframeContextMenu_) {
        ImGui::OpenPopup("KeyframeContextMenu");
        pendingKeyframeContextMenu_ = false;
    }
    if (ImGui::BeginPopup("KeyframeContextMenu")) {
        if (selectedTrack_ >= 0 && selectedKeyframe_ >= 0) {
            auto& track = data_->tracks_[selectedTrack_];
            auto& kf = track.keyframes[selectedKeyframe_];
            ImGui::Text("Keyframe @ frame %d (value=%.2f)", kf.frame, kf.value);
            ImGui::Separator();
            if (ImGui::MenuItem("Delete")) {
                data_->removeKeyframe(selectedTrack_, selectedKeyframe_);
                selectedKeyframe_ = -1;
            }
            ImGui::Separator();
            ImGui::Text("Easing:");
            for (int e = 0; e < 4; ++e) {
                if (ImGui::MenuItem(kEaseNames[e], nullptr, kf.easeType == e)) {
                    data_->setKeyframeEasing(selectedTrack_, selectedKeyframe_, e);
                }
            }
        }
        ImGui::EndPopup();
    }

    // 轨道右键菜单
    if (pendingTrackContextMenu_) {
        ImGui::OpenPopup("TrackContextMenu");
        pendingTrackContextMenu_ = false;
    }
    if (ImGui::BeginPopup("TrackContextMenu")) {
        if (selectedTrack_ >= 0) {
            ImGui::Text("Track: %s", data_->tracks_[selectedTrack_].name.c_str());
            ImGui::Separator();
            if (ImGui::MenuItem("Remove Track")) {
                data_->removeTrack(selectedTrack_);
                selectedTrack_ = -1;
                selectedKeyframe_ = -1;
            }
        }
        ImGui::EndPopup();
    }

    // 添加轨道弹出窗口
    if (ImGui::BeginPopup("AddTrackPopup")) {
        static char nameBuf[64] = "";
        static char nodeBuf[64] = "";
        static char propBuf[64] = "";
        ImGui::InputText("Track Name", nameBuf, 64);
        ImGui::InputText("Target Node", nodeBuf, 64);
        ImGui::InputText("Property", propBuf, 64);
        if (ImGui::Button("Create") && std::strlen(nameBuf) > 0) {
            data_->addTrack(nameBuf, nodeBuf, propBuf);
            std::memset(nameBuf, 0, 64);
            std::memset(nodeBuf, 0, 64);
            std::memset(propBuf, 0, 64);
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    ImGui::End();
}

// ================================================================
//  renderPlaybackControls — 播放控制栏
// ================================================================

void TimelineView::renderPlaybackControls() {
    // |<  First frame
    if (ImGui::Button("|<", ImVec2(28, 0))) {
        data_->seekTo(0);
    }
    ImGui::SameLine();

    // <   Previous frame
    if (ImGui::Button("<", ImVec2(28, 0))) {
        data_->seekTo(data_->currentFrame - 1);
    }
    ImGui::SameLine();

    // Play / Pause toggle
    const char* playLabel = data_->isPlaying ? "Pause" : "Play";
    if (ImGui::Button(playLabel, ImVec2(50, 0))) {
        if (data_->isPlaying) {
            data_->pause();
        } else {
            data_->play();
        }
    }
    ImGui::SameLine();

    // Stop (pause + go to 0)
    if (ImGui::Button("Stop", ImVec2(40, 0))) {
        data_->pause();
        data_->seekTo(0);
    }
    ImGui::SameLine();

    // >   Next frame
    if (ImGui::Button(">", ImVec2(28, 0))) {
        data_->seekTo(data_->currentFrame + 1);
    }
    ImGui::SameLine();

    // >|  Last frame
    if (ImGui::Button(">|", ImVec2(28, 0))) {
        data_->seekTo(data_->totalFrames);
    }
    ImGui::SameLine();

    // Frame counter
    ImGui::Text(" Frame: %d / %d   FPS: %d",
                data_->currentFrame, data_->totalFrames, data_->fps);
    ImGui::SameLine();

    // + Track button
    if (ImGui::Button("+ Track")) {
        ImGui::OpenPopup("AddTrackPopup");
    }
}

// ================================================================
//  renderTimeRuler — 时间标尺（帧刻度 + 播放头）
// ================================================================

void TimelineView::renderTimeRuler() {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 cursor = ImGui::GetCursorScreenPos();
    float availWidth = ImGui::GetContentRegionAvail().x;

    ImGui::InvisibleButton("##RulerBg", ImVec2(availWidth, kRulerHeight));
    ImVec2 rulerEnd(cursor.x + availWidth, cursor.y + kRulerHeight);

    // 标尺背景
    dl->AddRectFilled(cursor, rulerEnd, IM_COL32(38, 38, 42, 255));

    float originX = cursor.x;
    float contentStartX = originX + kHeaderWidth;

    // 绘制标题列背景
    dl->AddRectFilled(ImVec2(originX, cursor.y),
                      ImVec2(contentStartX, cursor.y + kRulerHeight),
                      IM_COL32(50, 50, 55, 255));

    // 绘制刻度线和帧号
    // 根据当前缩放确定哪些帧可见
    int firstVisibleFrame = static_cast<int>(std::max(0.0f, scrollX_ / pixelsPerFrame_));
    int lastVisibleFrame = static_cast<int>(std::ceil(
        (scrollX_ + (availWidth - kHeaderWidth)) / pixelsPerFrame_));
    lastVisibleFrame = std::min(lastVisibleFrame, data_->totalFrames + 1);

    // 确定合适的刻度间隔（根据缩放级别）
    int tickInterval = 1;
    if (pixelsPerFrame_ < 3.0f)       tickInterval = 20;
    else if (pixelsPerFrame_ < 6.0f) tickInterval = 10;
    else if (pixelsPerFrame_ < 12.0f) tickInterval = 5;
    else if (pixelsPerFrame_ < 25.0f) tickInterval = 2;

    for (int i = firstVisibleFrame; i <= lastVisibleFrame; ++i) {
        float x = frameToX(static_cast<float>(i), originX);

        bool isMajor = (i % (tickInterval * 2) == 0) || (tickInterval == 1 && i % 10 == 0);
        bool isMedium = (i % tickInterval == 0);
        float tickHeight = isMajor ? kRulerHeight * 0.7f : (isMedium ? kRulerHeight * 0.45f : kRulerHeight * 0.25f);
        ImU32 tickColor = isMajor ? IM_COL32(170, 170, 175, 255) :
                          (isMedium ? IM_COL32(110, 110, 115, 255) : IM_COL32(70, 70, 75, 255));

        dl->AddLine(ImVec2(x, cursor.y + kRulerHeight - tickHeight),
                    ImVec2(x, cursor.y + kRulerHeight), tickColor);

        if (isMajor) {
            char buf[16];
            std::snprintf(buf, sizeof(buf), "%d", i);
            dl->AddText(ImVec2(x + 2.0f, cursor.y + 2.0f),
                        IM_COL32(190, 190, 195, 255), buf);
        }
    }

    // 播放头 (红色竖线 + 顶部三角)
    float playheadX = frameToX(static_cast<float>(data_->currentFrame), originX);

    // 只在播放头可见时绘制
    if (playheadX >= contentStartX && playheadX <= rulerEnd.x) {
        dl->AddLine(ImVec2(playheadX, cursor.y),
                    ImVec2(playheadX, cursor.y + kRulerHeight),
                    IM_COL32(255, 60, 60, 255), 2.0f);

        ImVec2 tri[3] = {
            ImVec2(playheadX - 6.0f, cursor.y),
            ImVec2(playheadX + 6.0f, cursor.y),
            ImVec2(playheadX, cursor.y + 7.0f)
        };
        dl->AddTriangleFilled(tri[0], tri[1], tri[2], IM_COL32(255, 80, 80, 255));
    }
}

// ================================================================
//  renderTrackList — 轨道列表 + 关键帧钻石
// ================================================================

void TimelineView::renderTrackList() {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 cursor = ImGui::GetCursorScreenPos();
    float availWidth = ImGui::GetContentRegionAvail().x;

    int trackCount = data_->getTrackCount();

    // 轨道区域背景 InvisibleButton（捕获鼠标事件）
    float totalRowHeight = std::max(kTrackHeight, kTrackHeight * static_cast<float>(trackCount));
    float contentHeight = std::max(totalRowHeight, 100.0f);
    ImGui::InvisibleButton("##TrackAreaBg", ImVec2(availWidth, contentHeight));

    ImVec2 areaMin = ImGui::GetItemRectMin();
    ImVec2 areaMax = ImGui::GetItemRectMax();
    bool areaHovered = ImGui::IsItemHovered();
    ImVec2 mousePos = ImGui::GetMousePos();

    float originX = areaMin.x;
    float contentStartX = originX + kHeaderWidth;

    // 重置拖拽起始值
    static int lastDragTrack = -1;
    static int lastDragKf = -1;

    for (int ti = 0; ti < trackCount; ++ti) {
        auto& track = data_->tracks_[ti];
        float rowY = areaMin.y + ti * kTrackHeight;
        float lineY = rowY + kTrackHeight * 0.5f;

        bool isSelectedTrack = (ti == selectedTrack_);

        // --- 轨道头部 ---
        ImVec2 headerPos(originX, rowY);
        ImVec2 headerEnd(contentStartX, rowY + kTrackHeight);
        ImU32 headerBg = isSelectedTrack ? IM_COL32(65, 65, 75, 255)
                                         : IM_COL32(50, 50, 55, 255);
        dl->AddRectFilled(headerPos, headerEnd, headerBg);

        // 轨道名称
        dl->AddText(ImVec2(headerPos.x + 4, headerPos.y + 3),
                    IM_COL32(255, 255, 255, 255), track.name.c_str());

        // 目标节点.属性
        std::string targetInfo = track.targetNode + "." + track.property;
        dl->AddText(ImVec2(headerPos.x + 4, headerPos.y + 19),
                    IM_COL32(140, 140, 145, 255), targetInfo.c_str());

        // 曲线预览缩略图
        if (track.keyframes.size() >= 2) {
            renderCurvePreview(dl,
                ImVec2(headerPos.x + kHeaderWidth - 72, headerPos.y + 5),
                ImVec2(66, 26), ti);
        }

        // 头部分隔线
        dl->AddLine(ImVec2(contentStartX, rowY),
                    ImVec2(contentStartX, rowY + kTrackHeight),
                    IM_COL32(70, 70, 75, 255));

        // --- 关键帧区域 ---
        ImVec2 keyAreaMin(contentStartX, rowY);
        ImVec2 keyAreaMax(areaMax.x, rowY + kTrackHeight);
        ImU32 keyAreaBg = isSelectedTrack ? IM_COL32(52, 52, 58, 255)
                                          : IM_COL32(42, 42, 46, 255);
        dl->AddRectFilled(keyAreaMin, keyAreaMax, keyAreaBg);

        // 轨道中线
        dl->AddLine(ImVec2(std::max(keyAreaMin.x, originX + kHeaderWidth), lineY),
                    ImVec2(keyAreaMax.x, lineY),
                    IM_COL32(75, 75, 80, 255));

        // 轨道底部线
        dl->AddLine(ImVec2(originX, rowY + kTrackHeight),
                    ImVec2(keyAreaMax.x, rowY + kTrackHeight),
                    IM_COL32(55, 55, 60, 255));

        // --- 绘制关键帧钻石 ---
        int kfCount = static_cast<int>(track.keyframes.size());
        for (int ki = 0; ki < kfCount; ++ki) {
            auto& kf = track.keyframes[ki];
            float kfX = frameToX(static_cast<float>(kf.frame), originX);

            // 视锥剔除
            if (kfX < contentStartX - kKeyframeHitRadius ||
                kfX > areaMax.x + kKeyframeHitRadius) continue;

            bool isSelectedKf = (ti == selectedTrack_ && ki == selectedKeyframe_);
            float half = kKeyframeHalfSize;
            int easeIdx = std::max(0, std::min(kf.easeType, 3));

            // 外层菱形 (缓动颜色环)
            float outerHalf = half + 2.5f;
            ImVec2 outer[4] = {
                ImVec2(kfX, lineY - outerHalf),
                ImVec2(kfX + outerHalf, lineY),
                ImVec2(kfX, lineY + outerHalf),
                ImVec2(kfX - outerHalf, lineY)
            };
            dl->AddQuadFilled(outer[0], outer[1], outer[2], outer[3],
                              kEaseColors[easeIdx]);

            // 内层菱形 (主体)
            ImU32 fillColor = isSelectedKf ? IM_COL32(255, 200, 50, 255)
                                           : IM_COL32(240, 240, 245, 255);
            ImVec2 inner[4] = {
                ImVec2(kfX, lineY - half),
                ImVec2(kfX + half, lineY),
                ImVec2(kfX, lineY + half),
                ImVec2(kfX - half, lineY)
            };
            dl->AddQuadFilled(inner[0], inner[1], inner[2], inner[3], fillColor);

            // 选中高亮环
            if (isSelectedKf) {
                dl->AddQuad(outer[0], outer[1], outer[2], outer[3],
                            IM_COL32(255, 255, 100, 255), 1.5f);
            }
        }

        // --- 鼠标交互（仅在悬停轨道区域时处理） ---
        if (areaHovered) {
            // 检查是否在关键帧上右击
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
                bool hitAnyKf = false;
                for (int ki = kfCount - 1; ki >= 0; --ki) {
                    auto& kf = track.keyframes[ki];
                    float kfX = frameToX(static_cast<float>(kf.frame), originX);
                    if (kfX < contentStartX || kfX > areaMax.x) continue;
                    if (hitTestKeyframe(mousePos.x, mousePos.y, kfX, lineY)) {
                        selectedTrack_ = ti;
                        selectedKeyframe_ = ki;
                        pendingKeyframeContextMenu_ = true;
                        hitAnyKf = true;
                        break;
                    }
                }
                // 没命中关键帧，检查是否在轨道头部右键
                if (!hitAnyKf &&
                    mousePos.x >= headerPos.x && mousePos.x <= headerEnd.x &&
                    mousePos.y >= headerPos.y && mousePos.y <= headerEnd.y) {
                    selectedTrack_ = ti;
                    selectedKeyframe_ = -1;
                    pendingTrackContextMenu_ = true;
                }
            }

            // 检查是否在关键帧上左击
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                bool hitAnyKf = false;
                for (int ki = kfCount - 1; ki >= 0; --ki) {
                    auto& kf = track.keyframes[ki];
                    float kfX = frameToX(static_cast<float>(kf.frame), originX);
                    if (kfX < contentStartX || kfX > areaMax.x) continue;
                    if (hitTestKeyframe(mousePos.x, mousePos.y, kfX, lineY)) {
                        selectedTrack_ = ti;
                        selectedKeyframe_ = ki;
                        draggingKeyframe_ = true;
                        dragTrack_ = ti;
                        dragKeyframeIndex_ = ki;
                        dragStartFrame_ = kf.frame;
                        dragStartValue_ = kf.value;
                        hitAnyKf = true;
                        break;
                    }
                }
                if (!hitAnyKf) {
                    selectedTrack_ = -1;
                    selectedKeyframe_ = -1;
                }
            }

            // 双击空白区域添加关键帧
            if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                bool hitAnyKf = false;
                for (int ki = 0; ki < kfCount; ++ki) {
                    auto& kf = track.keyframes[ki];
                    float kfX = frameToX(static_cast<float>(kf.frame), originX);
                    if (hitTestKeyframe(mousePos.x, mousePos.y, kfX, lineY)) {
                        hitAnyKf = true;
                        break;
                    }
                }
                if (!hitAnyKf &&
                    mousePos.x >= contentStartX && mousePos.x <= areaMax.x &&
                    mousePos.y >= rowY && mousePos.y <= rowY + kTrackHeight) {
                    int newFrame = static_cast<int>(std::round(
                        xToFrame(mousePos.x, originX)));
                    newFrame = std::max(0, std::min(newFrame, data_->totalFrames));
                    data_->addKeyframe(ti, newFrame, 0.0f);
                    // 选中新关键帧
                    selectedTrack_ = ti;
                    selectedKeyframe_ = static_cast<int>(track.keyframes.size()) - 1;
                }
            }
        }

        // --- 关键帧拖拽 ---
        if (draggingKeyframe_ && dragTrack_ == ti) {
            if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
                draggingKeyframe_ = false;
                dragTrack_ = -1;
                dragKeyframeIndex_ = -1;
            } else if (ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
                int newFrame = static_cast<int>(std::round(
                    xToFrame(mousePos.x, originX)));
                newFrame = std::max(0, std::min(newFrame, data_->totalFrames));

                // 垂直拖拽：每像素对应 0.5 的值变化
                float deltaY = -(mousePos.y - (dragStartValue_ > 0 ? lineY : lineY));
                float newValue = dragStartValue_ + ImGui::GetIO().MouseDelta.y * -0.1f;

                data_->moveKeyframe(ti, dragKeyframeIndex_, newFrame, newValue);
                dragStartValue_ = newValue;
                selectedTrack_ = ti;
                selectedKeyframe_ = dragKeyframeIndex_;
            }
        }
    }
}

// ================================================================
//  renderCurvePreview — 轨道头部的小型曲线预览
// ================================================================

void TimelineView::renderCurvePreview(ImDrawList* dl, ImVec2 pos, ImVec2 size,
                                       int trackIndex) {
    if (trackIndex < 0 || trackIndex >= data_->getTrackCount()) return;
    const auto& kfs = data_->tracks_[trackIndex].keyframes;
    if (kfs.size() < 2) return;

    // 背景
    dl->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y),
                      IM_COL32(28, 28, 32, 255));
    dl->AddRect(pos, ImVec2(pos.x + size.x, pos.y + size.y),
                IM_COL32(60, 60, 65, 255));

    // 计算值范围
    int minFrame = kfs.front().frame;
    int maxFrame = kfs.back().frame;
    float minVal = kfs[0].value;
    float maxVal = kfs[0].value;
    for (const auto& kf : kfs) {
        if (kf.value < minVal) minVal = kf.value;
        if (kf.value > maxVal) maxVal = kf.value;
    }
    float valRange = maxVal - minVal;
    if (valRange < 1.0f) valRange = 1.0f;

    int frameRange = maxFrame - minFrame;
    if (frameRange <= 0) frameRange = 1;

    // 绘制曲线
    int numSamples = static_cast<int>(size.x);
    for (int sx = 1; sx < numSamples; ++sx) {
        int frame0 = minFrame + (sx - 1) * frameRange / numSamples;
        int frame1 = minFrame + sx * frameRange / numSamples;
        float val0 = data_->interpolate(trackIndex, frame0);
        float val1 = data_->interpolate(trackIndex, frame1);

        float px0 = pos.x + static_cast<float>(sx - 1) / numSamples * size.x;
        float py0 = pos.y + size.y - ((val0 - minVal) / valRange) * size.y;
        float px1 = pos.x + static_cast<float>(sx) / numSamples * size.x;
        float py1 = pos.y + size.y - ((val1 - minVal) / valRange) * size.y;

        dl->AddLine(ImVec2(px0, py0), ImVec2(px1, py1),
                    IM_COL32(100, 200, 255, 255), 1.2f);
    }

    // 关键帧标记点
    for (const auto& kf : kfs) {
        float ratio = static_cast<float>(kf.frame - minFrame) / frameRange;
        float kx = pos.x + ratio * size.x;
        float ky = pos.y + size.y - ((kf.value - minVal) / valRange) * size.y;
        dl->AddCircleFilled(ImVec2(kx, ky), 1.5f,
                            IM_COL32(255, 200, 100, 255));
    }
}

// ================================================================
//  TimelineController 实现
// ================================================================

bool TimelineController::initialize(const std::unordered_map<std::string, std::any>& config) {
    auto it = config.find("data");
    if (it == config.end()) {
        CCLOG("[TimelineController] missing 'data' in config");
        return false;
    }
    data_ = std::any_cast<TimelineData*>(it->second);
    if (!data_) {
        CCLOG("[TimelineController] null data pointer");
        return false;
    }
    lastFrame_ = data_->currentFrame;
    lastIsPlaying_ = data_->isPlaying;
    CCLOG("[TimelineController] initialized");
    return true;
}

void TimelineController::update(float) {
    if (!data_) return;

    // 播放时推进帧
    if (data_->isPlaying) {
        data_->advance();
    }

    // 检测帧变化 — 发布 FrameChangedEvent（覆盖自动播放和手动 seek）
    if (data_->currentFrame != lastFrame_) {
        FrameChangedEvent event{data_->currentFrame};
        ugf::EventBus::getInstance().publish(event);

        if (data_->currentFrame > 0 && data_->currentFrame % 10 == 0) {
            CCLOG("[Timeline] Frame: %d", data_->currentFrame);
        }

        lastFrame_ = data_->currentFrame;
    }

    // 检测播放状态变化 — 发布 PlayStateChangedEvent
    if (data_->isPlaying != lastIsPlaying_) {
        PlayStateChangedEvent event{data_->isPlaying};
        ugf::EventBus::getInstance().publish(event);
        lastIsPlaying_ = data_->isPlaying;
    }
}

void TimelineController::terminate() {
    CCLOG("[TimelineController] terminated");
}

// ================================================================
//  TimelinePlugin 实现
// ================================================================

bool TimelinePlugin::initialize() {
    // 注册 TimelineData 组件
    auto* data = componentSystem.registerComponent<TimelineData>("data");
    if (!data) {
        CCLOG("[TimelinePlugin] Failed to register TimelineData");
        return false;
    }

    // 注册 TimelineView 组件 — 传入 data 指针
    std::unordered_map<std::string, std::any> viewCfg;
    viewCfg["data"] = data;
    auto* view = componentSystem.registerComponent<TimelineView>("view", viewCfg);
    if (!view) {
        CCLOG("[TimelinePlugin] Failed to register TimelineView");
        return false;
    }

    // 注册 TimelineController 组件 — 传入 data 指针
    std::unordered_map<std::string, std::any> ctrlCfg;
    ctrlCfg["data"] = data;
    auto* controller = componentSystem.registerComponent<TimelineController>("controller", ctrlCfg);
    if (!controller) {
        CCLOG("[TimelinePlugin] Failed to register TimelineController");
        return false;
    }

    // 运行 API 测试
    TestFramework::getInstance().runApiTests("Timeline");

    CCLOG("[TimelinePlugin] initialized (3 components: data, view, controller)");
    return true;
}

void TimelinePlugin::update(float deltaTime) {
    // 驱动所有组件按注册顺序更新: data → view → controller
    componentSystem.updateAll(deltaTime);

    // 运行 GUI 测试
    TestFramework::getInstance().runGuiTests("Timeline");
}

void TimelinePlugin::shutdown() {
    componentSystem.clear();
    CCLOG("[TimelinePlugin] shutdown");
}
