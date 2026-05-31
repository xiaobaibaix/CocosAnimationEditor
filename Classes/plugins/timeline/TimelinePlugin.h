// TimelinePlugin.h - 时间轴插件（UGF 多组件架构）
// TimelineData   : ugf::Component — 数据模型
// TimelineView   : ugf::Component — ImGui 视图
// TimelineController : ugf::Component — 逻辑控制
// TimelinePlugin : ugf::IPlugin     — 插件入口
#ifndef TIMELINE_PLUGIN_H
#define TIMELINE_PLUGIN_H

#include "PluginSystem.hpp"
#include "ComponentSystem.hpp"
#include "editor/EditorEvents.h"
#include "imgui.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <any>

// ================================================================
//  Keyframe — 单个关键帧
//  easeType: 0=Linear, 1=EaseIn, 2=EaseOut, 3=EaseInOut
// ================================================================
struct Keyframe {
    int frame = 0;
    float value = 0.0f;
    int easeType = 0;
};

// ================================================================
//  Track — 单条动画轨道，绑定到节点属性
// ================================================================
struct Track {
    std::string name;
    std::string targetNode;
    std::string property;
    std::vector<Keyframe> keyframes;
};

// ================================================================
//  TimelineData — 数据模型组件
//  存储动画播放状态：帧数、当前帧、播放状态、帧率、轨道列表
// ================================================================
class TimelineData : public ugf::Component {
public:
    std::string getComponentId() const override { return "TimelineData"; }
    bool initialize(const std::unordered_map<std::string, std::any>& config = {}) override;
    void update(float deltaTime) override;
    void terminate() override;

    int totalFrames = 60;
    int currentFrame = 0;
    bool isPlaying = false;
    int fps = 30;

    void play();
    void pause();
    void seekTo(int frame);
    void advance();   // 前进一帧，到达 totalFrames 时循环到 0

    // --- 轨道管理 ---
    std::vector<Track> tracks_;

    void addTrack(const std::string& name, const std::string& targetNode,
                  const std::string& property);
    void removeTrack(int trackIndex);
    int getTrackCount() const { return static_cast<int>(tracks_.size()); }

    // --- 关键帧管理 ---
    void addKeyframe(int trackIndex, int frame, float value, int easeType = 0);
    void removeKeyframe(int trackIndex, int keyframeIndex);
    void moveKeyframe(int trackIndex, int keyframeIndex, int newFrame, float newValue);
    void setKeyframeEasing(int trackIndex, int keyframeIndex, int easeType);
    int getKeyframeCount() const;
    float interpolate(int trackIndex, int frame) const;

private:
    int clampFrame(int frame) const;
};

// ================================================================
//  TimelineView — ImGui 视图组件
//  渲染时间轴窗口：时间标尺、播放头、轨道列表、关键帧钻石标记
//  通过 config["data"] 获取 TimelineData 指针
// ================================================================
class TimelineView : public ugf::Component {
public:
    std::string getComponentId() const override { return "TimelineView"; }
    bool initialize(const std::unordered_map<std::string, std::any>& config = {}) override;
    void update(float deltaTime) override;
    void terminate() override;

private:
    void renderTimelineWindow();

    // 子区域渲染
    void renderPlaybackControls();
    void renderTimeRuler();
    void renderTrackList();
    void renderCurvePreview(ImDrawList* dl, ImVec2 pos, ImVec2 size, int trackIndex);

    // 坐标转换
    float frameToX(float frame, float originX) const;
    float xToFrame(float x, float originX) const;

    // 关键帧命中测试
    bool hitTestKeyframe(float mouseX, float mouseY, float kfX, float lineY) const;

    TimelineData* data_ = nullptr;
    bool windowOpen_ = true;

    // 缩放与滚动
    float pixelsPerFrame_ = 10.0f;
    float scrollX_ = 0.0f;

    // 选中状态
    int selectedTrack_ = -1;
    int selectedKeyframe_ = -1;

    // 右键菜单触发标志
    bool pendingKeyframeContextMenu_ = false;
    bool pendingTrackContextMenu_ = false;

    // 关键帧拖拽状态
    bool draggingKeyframe_ = false;
    int dragTrack_ = -1;
    int dragKeyframeIndex_ = -1;
    int dragStartFrame_ = 0;
    float dragStartValue_ = 0.0f;

    // 常量
    static constexpr float kHeaderWidth = 180.0f;
    static constexpr float kTrackHeight = 36.0f;
    static constexpr float kRulerHeight = 32.0f;
    static constexpr float kKeyframeHalfSize = 5.0f;
    static constexpr float kKeyframeHitRadius = 8.0f;
};

// ================================================================
//  TimelineController — 逻辑控制组件
//  驱动播放时帧前进，检测状态变化并发布事件
//  通过 config["data"] 获取 TimelineData 指针
// ================================================================
class TimelineController : public ugf::Component {
public:
    std::string getComponentId() const override { return "TimelineController"; }
    bool initialize(const std::unordered_map<std::string, std::any>& config = {}) override;
    void update(float deltaTime) override;
    void terminate() override;

private:
    TimelineData* data_ = nullptr;
    int lastFrame_ = 0;
    bool lastIsPlaying_ = false;
};

// ================================================================
//  TimelinePlugin — 插件入口
//  注册 Data → View → Controller 三个组件到 ComponentSystem
// ================================================================
class TimelinePlugin : public ugf::IPlugin {
public:
    std::string getId() const override { return "Timeline"; }
    bool initialize() override;
    void update(float deltaTime) override;
    void shutdown() override;
};

#endif // TIMELINE_PLUGIN_H
