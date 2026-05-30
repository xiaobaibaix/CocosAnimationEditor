// TimelinePlugin.cpp - 时间轴插件实现（多组件架构）
#include "TimelinePlugin.h"
#include "imgui.h"
#include "cocos2d.h"
#include "EventSystem.hpp"
#include "tests/TestFramework.h"

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
    if (frame < 0) frame = 0;
    if (frame > totalFrames) frame = totalFrames;
    currentFrame = frame;
}

void TimelineData::advance() {
    currentFrame++;
    if (currentFrame > totalFrames) {
        currentFrame = 0;
    }
}

// ================================================================
//  TimelineView 实现
// ================================================================

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

void TimelineView::renderTimelineWindow() {
    ImGui::Begin("Timeline", &windowOpen_);

    // --- 播放控制栏 ---
    const char* buttonLabel = data_->isPlaying ? "Pause" : "Play";
    if (ImGui::Button(buttonLabel, ImVec2(60, 0))) {
        if (data_->isPlaying) {
            data_->pause();
        } else {
            data_->play();
        }
    }

    ImGui::SameLine();
    ImGui::Text("Frame: %d/%d", data_->currentFrame, data_->totalFrames);

    // --- 时间标尺 ---
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 cursor = ImGui::GetCursorScreenPos();
    float availWidth = ImGui::GetContentRegionAvail().x;
    float rulerHeight = 36.0f;

    // 为标尺区域预留空间
    ImGui::InvisibleButton("RulerBg", ImVec2(availWidth, rulerHeight));

    // 标尺背景
    ImVec2 rulerEnd(cursor.x + availWidth, cursor.y + rulerHeight);
    drawList->AddRectFilled(cursor, rulerEnd, IM_COL32(45, 45, 48, 255));

    // 绘制帧刻度线（每 10 帧一条长线）和帧号
    for (int i = 0; i <= data_->totalFrames; i++) {
        float x = cursor.x + (float)i / (float)data_->totalFrames * availWidth;
        float tickHeight = (i % 10 == 0) ? rulerHeight * 0.7f : rulerHeight * 0.35f;
        ImU32 tickColor = (i % 10 == 0) ? IM_COL32(180, 180, 180, 255) : IM_COL32(100, 100, 100, 255);
        drawList->AddLine(
            ImVec2(x, cursor.y + rulerHeight - tickHeight),
            ImVec2(x, cursor.y + rulerHeight),
            tickColor);

        // 每 10 帧标注帧号
        if (i % 10 == 0) {
            char buf[8];
            snprintf(buf, sizeof(buf), "%d", i);
            drawList->AddText(ImVec2(x + 2.0f, cursor.y + 2.0f),
                              IM_COL32(200, 200, 200, 255), buf);
        }
    }

    // 红色播放头 (垂直竖线 + 顶部三角形)
    float playheadX = cursor.x + (float)data_->currentFrame / (float)data_->totalFrames * availWidth;
    drawList->AddLine(
        ImVec2(playheadX, cursor.y),
        ImVec2(playheadX, cursor.y + rulerHeight),
        IM_COL32(255, 60, 60, 255), 2.0f);

    ImVec2 triPts[3] = {
        ImVec2(playheadX - 5.0f, cursor.y),
        ImVec2(playheadX + 5.0f, cursor.y),
        ImVec2(playheadX, cursor.y + 6.0f)
    };
    drawList->AddTriangleFilled(triPts[0], triPts[1], triPts[2], IM_COL32(255, 80, 80, 255));

    // --- 帧跳转输入 ---
    ImGui::Spacing();
    ImGui::SetNextItemWidth(100.0f);
    int frameInput = data_->currentFrame;
    if (ImGui::InputInt("Go to Frame", &frameInput, 1, 10)) {
        data_->seekTo(frameInput);
    }

    ImGui::End();
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

        // 每 10 帧打印日志
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
