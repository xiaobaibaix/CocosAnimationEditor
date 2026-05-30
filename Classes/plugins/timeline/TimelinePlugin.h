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
#include <string>
#include <unordered_map>
#include <any>

// ================================================================
//  TimelineData — 数据模型组件
//  存储动画播放状态：帧数、当前帧、播放状态、帧率
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
};

// ================================================================
//  TimelineView — ImGui 视图组件
//  渲染时间轴窗口：时间标尺、播放头、播放/暂停按钮、帧计数器
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

    TimelineData* data_ = nullptr;
    bool windowOpen_ = true;
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
