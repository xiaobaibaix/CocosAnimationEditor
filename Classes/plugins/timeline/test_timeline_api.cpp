// test_timeline_api.cpp - TimelineData 组件 API 测试 + GUI 测试
#include "TimelinePlugin.h"
#include "tests/TestFramework.h"
#include "imgui.h"
#include "cocos2d.h"
#include <cmath>

// ================================================================
//  API 测试 — TimelineData 基础功能验证
// ================================================================

static bool test_timeline_create() {
    TimelineData data;
    bool ok = data.initialize();
    if (!ok) return false;
    if (data.getComponentId() != "TimelineData") return false;
    if (data.totalFrames != 60) return false;
    if (data.currentFrame != 0) return false;
    if (data.isPlaying) return false;
    if (data.fps != 30) return false;
    data.terminate();
    return true;
}

static bool test_timeline_seek() {
    TimelineData data;
    data.initialize();

    data.seekTo(30);
    if (data.currentFrame != 30) return false;

    // 边界：负数 clamp 到 0
    data.seekTo(-5);
    if (data.currentFrame != 0) return false;

    // 边界：超过 totalFrames clamp 到 totalFrames
    data.seekTo(100);
    if (data.currentFrame != data.totalFrames) return false;

    // 边界：精确等于 totalFrames
    data.seekTo(60);
    if (data.currentFrame != 60) return false;

    data.terminate();
    return true;
}

static bool test_timeline_play_pause() {
    TimelineData data;
    data.initialize();

    if (data.isPlaying) return false;
    data.play();
    if (!data.isPlaying) return false;
    data.pause();
    if (data.isPlaying) return false;

    data.terminate();
    return true;
}

static bool test_timeline_advance() {
    TimelineData data;
    data.initialize();

    data.advance();
    if (data.currentFrame != 1) return false;

    for (int i = 0; i < 9; i++) data.advance();
    if (data.currentFrame != 10) return false;

    // 循环回 0
    data.seekTo(data.totalFrames - 1);
    data.advance();
    if (data.currentFrame != 60) return false;
    data.advance();
    if (data.currentFrame != 0) return false;

    data.terminate();
    return true;
}

static bool test_timeline_fps() {
    TimelineData data;
    data.initialize();

    if (data.fps != 30) return false;
    data.fps = 60;
    if (data.fps != 60) return false;

    data.terminate();
    return true;
}

// ================================================================
//  API 测试 — 轨道管理
// ================================================================

static bool test_track_add_remove() {
    TimelineData data;
    data.initialize();

    if (data.getTrackCount() != 0) return false;

    data.addTrack("Position", "Character", "position");
    data.addTrack("Rotation", "Character", "rotation");
    data.addTrack("Scale", "Character", "scale");

    if (data.getTrackCount() != 3) return false;
    if (data.tracks_[0].name != "Position") return false;
    if (data.tracks_[0].targetNode != "Character") return false;
    if (data.tracks_[0].property != "position") return false;

    // 移除中间轨道
    data.removeTrack(1);
    if (data.getTrackCount() != 2) return false;
    if (data.tracks_[0].name != "Position") return false;
    if (data.tracks_[1].name != "Scale") return false;

    // 移除所有
    data.removeTrack(0);
    data.removeTrack(0);
    if (data.getTrackCount() != 0) return false;

    // 边界：移除无效索引不应崩溃
    data.removeTrack(-1);
    data.removeTrack(0);
    data.removeTrack(999);

    data.terminate();
    return true;
}

// ================================================================
//  API 测试 — 关键帧管理
// ================================================================

static bool test_keyframe_add_remove() {
    TimelineData data;
    data.initialize();
    data.addTrack("Position", "Node", "x");

    // 添加关键帧（自动按帧号排序）
    data.addKeyframe(0, 30, 100.0f);
    data.addKeyframe(0, 10, 50.0f);
    data.addKeyframe(0, 60, 0.0f);

    auto& kfs = data.tracks_[0].keyframes;
    if (kfs.size() != 3) return false;

    // 验证排序：frame 10 -> 30 -> 60
    if (kfs[0].frame != 10 || kfs[0].value != 50.0f) return false;
    if (kfs[1].frame != 30 || kfs[1].value != 100.0f) return false;
    if (kfs[2].frame != 60 || kfs[2].value != 0.0f) return false;

    // 验证默认缓动
    if (kfs[0].easeType != 0) return false;

    // 移除中间的关键帧
    data.removeKeyframe(0, 1);
    if (kfs.size() != 2) return false;
    if (kfs[0].frame != 10) return false;
    if (kfs[1].frame != 60) return false;

    // 边界：移除无效索引不应崩溃
    data.removeKeyframe(0, 999);
    data.removeKeyframe(0, -1);
    data.removeKeyframe(999, 0);

    if (kfs.size() != 2) return false; // 不应变化

    data.terminate();
    return true;
}

static bool test_keyframe_move() {
    TimelineData data;
    data.initialize();
    data.addTrack("Position", "Node", "x");
    data.addKeyframe(0, 0, 0.0f);
    data.addKeyframe(0, 30, 100.0f);

    // 移动第一个关键帧
    data.moveKeyframe(0, 0, 15, 50.0f);

    auto& kfs = data.tracks_[0].keyframes;
    if (kfs.size() != 2) return false;
    // 帧 15 应该在 30 之前
    if (kfs[0].frame != 15 || kfs[0].value != 50.0f) return false;
    if (kfs[1].frame != 30 || kfs[1].value != 100.0f) return false;

    // 移动关键帧到超出最大值，应 clamp
    data.moveKeyframe(0, 0, 999, 999.0f);
    if (kfs[0].frame != data.totalFrames) return false;

    // 边界：移动无效索引不应崩溃
    data.moveKeyframe(0, 999, 0, 0.0f);
    data.moveKeyframe(999, 0, 0, 0.0f);

    data.terminate();
    return true;
}

static bool test_keyframe_easing() {
    TimelineData data;
    data.initialize();
    data.addTrack("Scale", "Node", "scale");
    data.addKeyframe(0, 0, 1.0f, 2); // EaseOut

    auto& kfs = data.tracks_[0].keyframes;
    if (kfs[0].easeType != 2) return false;

    // 修改缓动
    data.setKeyframeEasing(0, 0, 3); // EaseInOut
    if (kfs[0].easeType != 3) return false;

    data.setKeyframeEasing(0, 0, 1); // EaseIn
    if (kfs[0].easeType != 1) return false;

    data.setKeyframeEasing(0, 0, 0); // Linear
    if (kfs[0].easeType != 0) return false;

    // 边界：无效 easing 应 clamp
    data.setKeyframeEasing(0, 0, 99);
    if (kfs[0].easeType != 3) return false; // clamp to 3

    data.setKeyframeEasing(0, 0, -5);
    if (kfs[0].easeType != 0) return false; // clamp to 0

    // 边界：无效索引不应崩溃
    data.setKeyframeEasing(999, 0, 1);
    data.setKeyframeEasing(0, 999, 1);

    data.terminate();
    return true;
}

// ================================================================
//  API 测试 — 插值计算
// ================================================================

static bool test_interpolation() {
    TimelineData data;
    data.initialize();
    data.addTrack("Position", "Node", "x");
    data.addKeyframe(0, 0, 0.0f);
    data.addKeyframe(0, 10, 100.0f);
    data.addKeyframe(0, 20, 0.0f);

    // 线性插值：中点
    float v = data.interpolate(0, 5);
    if (std::abs(v - 50.0f) > 0.01f) return false;

    // 端点：精确值
    v = data.interpolate(0, 0);
    if (std::abs(v - 0.0f) > 0.01f) return false;

    v = data.interpolate(0, 10);
    if (std::abs(v - 100.0f) > 0.01f) return false;

    // 边界：超出范围应返回端点值
    v = data.interpolate(0, -5);
    if (std::abs(v - 0.0f) > 0.01f) return false;

    v = data.interpolate(0, 999);
    if (std::abs(v - 0.0f) > 0.01f) return false;

    // 空轨道
    data.addTrack("Empty", "Node", "x");
    v = data.interpolate(1, 5);
    if (std::abs(v - 0.0f) > 0.01f) return false;

    // 单关键帧轨道
    data.addTrack("Single", "Node", "x");
    data.addKeyframe(2, 5, 42.0f);
    v = data.interpolate(2, 0);
    if (std::abs(v - 42.0f) > 0.01f) return false;
    v = data.interpolate(2, 10);
    if (std::abs(v - 42.0f) > 0.01f) return false;

    data.terminate();
    return true;
}

static bool test_interpolation_easing() {
    TimelineData data;
    data.initialize();
    data.addTrack("Ease", "Node", "x");

    // EaseIn (t^2): 值在左半段增长慢
    data.addKeyframe(0, 0, 0.0f, 1); // EaseIn
    data.addKeyframe(0, 10, 100.0f);

    float vLinear = data.interpolate(0, 5);
    // 先改为线性测一下范围
    data.setKeyframeEasing(0, 0, 0);
    vLinear = data.interpolate(0, 5);
    if (std::abs(vLinear - 50.0f) > 0.01f) return false;

    // EaseIn: t=0.5 -> t^2=0.25, 所以值 = 25
    data.setKeyframeEasing(0, 0, 1);
    float vEaseIn = data.interpolate(0, 5);
    if (std::abs(vEaseIn - 25.0f) > 0.01f) return false;

    // EaseOut: t=0.5 -> t*(2-t)=0.75, 所以值 = 75
    data.setKeyframeEasing(0, 0, 2);
    float vEaseOut = data.interpolate(0, 5);
    if (std::abs(vEaseOut - 75.0f) > 0.01f) return false;

    // EaseInOut: 前半段用 2t^2 -> t=0.5, 2*(0.5)^2=0.5, 值 = 50
    data.setKeyframeEasing(0, 0, 3);
    float vEaseInOut = data.interpolate(0, 5);
    if (std::abs(vEaseInOut - 50.0f) > 0.01f) return false;

    data.terminate();
    return true;
}

// ================================================================
//  API 测试 — 播放状态转换
// ================================================================

static bool test_playback_state_transitions() {
    TimelineData data;
    data.initialize();

    // 初始状态
    if (data.isPlaying) return false;
    if (data.currentFrame != 0) return false;

    // play -> 播放中
    data.play();
    if (!data.isPlaying) return false;

    // pause -> 暂停但保留帧位置
    data.advance();
    data.advance();
    int frameAfterAdv = data.currentFrame;
    data.pause();
    if (data.isPlaying) return false;
    if (data.currentFrame != frameAfterAdv) return false; // 帧位置应保留

    // play -> 继续播放
    data.play();
    if (!data.isPlaying) return false;

    // pause -> seek 到新位置 -> 仍为暂停
    data.pause();
    data.seekTo(42);
    if (data.isPlaying) return false;
    if (data.currentFrame != 42) return false;

    // play -> pause -> 循环（通过 advance 循环）
    data.play();
    data.seekTo(data.totalFrames);
    data.advance();
    if (data.currentFrame != 0) return false; // 应循环到 0
    if (!data.isPlaying) return false; // 仍为播放状态

    data.terminate();
    return true;
}

// ================================================================
//  API 测试 — 帧边界 clamping
// ================================================================

static bool test_frame_boundary_clamp() {
    TimelineData data;
    data.initialize();

    // seekTo 边界
    data.seekTo(-100);
    if (data.currentFrame != 0) return false;

    data.seekTo(data.totalFrames + 100);
    if (data.currentFrame != data.totalFrames) return false;

    data.seekTo(0);
    if (data.currentFrame != 0) return false;

    data.seekTo(data.totalFrames);
    if (data.currentFrame != data.totalFrames) return false;

    // addKeyframe 边界
    data.addTrack("Test", "Node", "x");
    data.addKeyframe(0, -10, 0.0f);
    if (data.tracks_[0].keyframes[0].frame != 0) return false; // clamped to 0

    data.addKeyframe(0, 999, 0.0f);
    auto& kfs = data.tracks_[0].keyframes;
    if (kfs.back().frame != data.totalFrames) return false; // clamped to totalFrames

    // moveKeyframe 边界
    data.moveKeyframe(0, 0, -5, 0.0f);
    if (kfs[0].frame != 0) return false;

    data.moveKeyframe(0, 0, 999, 0.0f);
    if (kfs[0].frame != data.totalFrames) return false;

    data.terminate();
    return true;
}

// ================================================================
//  API 测试 — getKeyframeCount 统计
// ================================================================

static bool test_keyframe_count() {
    TimelineData data;
    data.initialize();

    if (data.getKeyframeCount() != 0) return false;

    data.addTrack("T1", "A", "x");
    data.addKeyframe(0, 0, 0.0f);
    data.addKeyframe(0, 10, 1.0f);
    if (data.getKeyframeCount() != 2) return false;

    data.addTrack("T2", "B", "y");
    data.addKeyframe(1, 5, 50.0f);
    data.addKeyframe(1, 15, 60.0f);
    data.addKeyframe(1, 25, 70.0f);
    if (data.getKeyframeCount() != 5) return false;

    data.removeTrack(0); // 移除 T1（2 个关键帧）
    if (data.getKeyframeCount() != 3) return false;

    data.terminate();
    return true;
}

// 注册所有 API 测试
REGISTER_API_TEST("Timeline", test_timeline_create);
REGISTER_API_TEST("Timeline", test_timeline_seek);
REGISTER_API_TEST("Timeline", test_timeline_play_pause);
REGISTER_API_TEST("Timeline", test_timeline_advance);
REGISTER_API_TEST("Timeline", test_timeline_fps);
REGISTER_API_TEST("Timeline", test_track_add_remove);
REGISTER_API_TEST("Timeline", test_keyframe_add_remove);
REGISTER_API_TEST("Timeline", test_keyframe_move);
REGISTER_API_TEST("Timeline", test_keyframe_easing);
REGISTER_API_TEST("Timeline", test_interpolation);
REGISTER_API_TEST("Timeline", test_interpolation_easing);
REGISTER_API_TEST("Timeline", test_playback_state_transitions);
REGISTER_API_TEST("Timeline", test_frame_boundary_clamp);
REGISTER_API_TEST("Timeline", test_keyframe_count);

// ================================================================
//  GUI 测试 — 在 ImGui 窗口中渲染测试信息
// ================================================================

static void test_gui_timeline_info() {
    ImGui::Text("TimelineData Component Test");
    ImGui::Text("  totalFrames: 60 (default)");
    ImGui::Text("  fps: 30 (default)");
    ImGui::Text("  getComponentId(): TimelineData");
    ImGui::Text("  Controls: play(), pause(), seekTo(n), advance()");

    static TimelineData testData;
    static bool initialized = false;
    if (!initialized) {
        testData.initialize();
        initialized = true;
    }

    ImGui::Separator();
    ImGui::Text("Live Test:");
    ImGui::Text("  isPlaying: %s", testData.isPlaying ? "true" : "false");
    ImGui::Text("  currentFrame: %d / %d", testData.currentFrame, testData.totalFrames);
    ImGui::Text("  fps: %d", testData.fps);

    if (ImGui::Button("Play##GuiTest")) testData.play();
    ImGui::SameLine();
    if (ImGui::Button("Pause##GuiTest")) testData.pause();
    ImGui::SameLine();
    if (ImGui::Button("Advance##GuiTest")) testData.advance();
    ImGui::SameLine();
    if (ImGui::Button("Reset##GuiTest")) testData.seekTo(0);

    if (testData.isPlaying) testData.advance();

    float progress = (float)testData.currentFrame / (float)testData.totalFrames;
    ImGui::ProgressBar(progress, ImVec2(-1.0f, 0.0f));
}

// ================================================================
//  GUI 测试 — 播放控制
// ================================================================

static void test_gui_timeline_controls() {
    ImGui::Text("Playback Controls Test");
    ImGui::Text("Verify play/pause/stop state transitions work correctly.");

    static TimelineData data;
    static bool initialized = false;
    if (!initialized) {
        data.initialize();
        initialized = true;
    }

    ImGui::Separator();

    ImGui::Text("State: %s | Frame: %d/%d | FPS: %d",
                data.isPlaying ? "PLAYING" : "PAUSED",
                data.currentFrame, data.totalFrames, data.fps);

    // 控制按钮
    if (ImGui::Button("|< First##GuiCtrl")) data.seekTo(0);
    ImGui::SameLine();
    if (ImGui::Button("< Prev##GuiCtrl")) data.seekTo(data.currentFrame - 1);
    ImGui::SameLine();
    const char* playLabel = data.isPlaying ? "Pause" : "Play";
    if (ImGui::Button(playLabel)) {
        if (data.isPlaying) data.pause(); else data.play();
    }
    ImGui::SameLine();
    if (ImGui::Button("Stop##GuiCtrl")) {
        data.pause();
        data.seekTo(0);
    }
    ImGui::SameLine();
    if (ImGui::Button("Next >##GuiCtrl")) data.seekTo(data.currentFrame + 1);
    ImGui::SameLine();
    if (ImGui::Button(">| Last##GuiCtrl")) data.seekTo(data.totalFrames);

    ImGui::Separator();

    // FPS 控制
    ImGui::SetNextItemWidth(80.0f);
    int fpsInput = data.fps;
    if (ImGui::InputInt("FPS##GuiCtrl", &fpsInput, 1, 10)) {
        data.fps = std::max(1, std::min(fpsInput, 120));
    }

    ImGui::SameLine();
    ImGui::SetNextItemWidth(80.0f);
    int totalInput = data.totalFrames;
    if (ImGui::InputInt("Total Frames##GuiCtrl", &totalInput, 1, 10)) {
        data.totalFrames = std::max(1, std::min(totalInput, 9999));
        if (data.currentFrame > data.totalFrames) data.seekTo(data.totalFrames);
    }

    // 进度条
    float progress = (float)data.currentFrame / (float)data.totalFrames;
    ImGui::ProgressBar(progress, ImVec2(-1.0f, 0.0f));

    // 状态指示器
    ImVec4 stateColor = data.isPlaying ? ImVec4(0.2f, 0.8f, 0.2f, 1.0f) :
                                        ImVec4(0.6f, 0.6f, 0.2f, 1.0f);
    ImGui::PushStyleColor(ImGuiCol_Button, stateColor);
    ImGui::Button(data.isPlaying ? "  LIVE  " : "  IDLE  ");
    ImGui::PopStyleColor();
}

// ================================================================
//  GUI 测试 — 关键帧编辑器
// ================================================================

static void test_gui_keyframe_editor() {
    ImGui::Text("Keyframe Editor Test");
    ImGui::Text("Shows tracks with keyframes. Click diamonds to select.");
    ImGui::Text("Right-click for context menu. Double-click empty area to add keyframe.");

    static TimelineData data;
    static bool seeded = false;
    if (!seeded) {
        data.initialize();
        data.totalFrames = 120;
        data.addTrack("Position", "Character", "position");
        data.addTrack("Rotation", "Character", "rotation");
        data.addTrack("Scale", "Character", "scale");

        data.addKeyframe(0, 0, 0.0f);
        data.addKeyframe(0, 30, 100.0f, 1);
        data.addKeyframe(0, 60, 50.0f, 2);
        data.addKeyframe(0, 90, 150.0f, 3);
        data.addKeyframe(0, 120, 0.0f);

        data.addKeyframe(1, 0, 0.0f);
        data.addKeyframe(1, 60, 180.0f);
        data.addKeyframe(1, 120, 360.0f);

        data.addKeyframe(2, 0, 1.0f);
        data.addKeyframe(2, 60, 1.5f);
        data.addKeyframe(2, 120, 1.0f);

        seeded = true;
    }

    ImGui::Separator();

    ImGui::Text("Track Count: %d", data.getTrackCount());
    ImGui::Text("Total Keyframes: %d", data.getKeyframeCount());
    ImGui::Text("isPlaying: %s   Frame: %d/%d",
                data.isPlaying ? "true" : "false",
                data.currentFrame, data.totalFrames);

    ImGui::Separator();

    // 轨道详情
    for (int ti = 0; ti < data.getTrackCount(); ++ti) {
        auto& track = data.tracks_[ti];
        if (ImGui::TreeNode(track.name.c_str(), "%s -> %s.%s [%zu kf]",
                            track.name.c_str(), track.targetNode.c_str(),
                            track.property.c_str(), track.keyframes.size())) {

            ImGui::BeginTable("KfTable", 5,
                ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg);
            ImGui::TableSetupColumn("Index");
            ImGui::TableSetupColumn("Frame");
            ImGui::TableSetupColumn("Value");
            ImGui::TableSetupColumn("Easing");
            ImGui::TableSetupColumn("Interpolated");
            ImGui::TableHeadersRow();

            static const char* easeLabels[] = {"Linear", "EaseIn", "EaseOut", "EaseInOut"};

            for (int ki = 0; ki < (int)track.keyframes.size(); ++ki) {
                auto& kf = track.keyframes[ki];
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("%d", ki);
                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%d", kf.frame);
                ImGui::TableSetColumnIndex(2);
                ImGui::Text("%.2f", kf.value);
                ImGui::TableSetColumnIndex(3);
                ImGui::Text("%s", easeLabels[std::max(0, std::min(kf.easeType, 3))]);
                ImGui::TableSetColumnIndex(4);
                float interp = data.interpolate(ti, data.currentFrame);
                ImGui::Text("%.2f", interp);
            }

            ImGui::EndTable();

            // 插值预览进度条
            float interpVal = data.interpolate(ti, data.currentFrame);
            if (track.keyframes.size() >= 2) {
                auto& first = track.keyframes.front();
                auto& last = track.keyframes.back();
                float valRange = last.value - first.value;
                if (std::abs(valRange) < 0.001f) valRange = 1.0f;
                float normVal = (interpVal - first.value) / valRange;
                ImGui::Text("Interpolated at frame %d: %.2f", data.currentFrame, interpVal);
                ImGui::ProgressBar(std::max(0.0f, std::min(normVal, 1.0f)),
                                   ImVec2(-1.0f, 0.0f));
            }

            ImGui::TreePop();
        }
    }
}

// ================================================================
//  GUI 测试 — 轨道管理
// ================================================================

static void test_gui_track_management() {
    ImGui::Text("Track Management Test");
    ImGui::Text("Use buttons below to add/remove tracks and verify API.");

    static TimelineData data;
    static bool seeded = false;
    if (!seeded) {
        data.initialize();
        data.addTrack("Position", "Node", "position.x");
        data.addTrack("Opacity", "Node", "opacity");
        seeded = true;
    }

    ImGui::Separator();

    // 当前轨道列表
    ImGui::Text("Current Tracks (%d):", data.getTrackCount());
    for (int ti = 0; ti < data.getTrackCount(); ++ti) {
        auto& track = data.tracks_[ti];
        ImGui::BulletText("[%d] %s -> %s.%s (%zu keyframes)",
                          ti, track.name.c_str(),
                          track.targetNode.c_str(), track.property.c_str(),
                          track.keyframes.size());
    }

    ImGui::Separator();

    // 添加轨道
    ImGui::Text("Add Track:");
    static char nameBuf[32] = "NewTrack";
    static char nodeBuf[32] = "TargetNode";
    static char propBuf[32] = "position.x";

    ImGui::SetNextItemWidth(120.0f);
    ImGui::InputText("Name##Mgmt", nameBuf, 32);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(120.0f);
    ImGui::InputText("Node##Mgmt", nodeBuf, 32);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(120.0f);
    ImGui::InputText("Prop##Mgmt", propBuf, 32);
    ImGui::SameLine();

    if (ImGui::Button("Add Track##Mgmt") && strlen(nameBuf) > 0) {
        data.addTrack(nameBuf, nodeBuf, propBuf);
    }

    // 移除轨道
    ImGui::Separator();
    ImGui::Text("Remove Track:");
    ImGui::SetNextItemWidth(60.0f);
    static int removeIdx = 0;
    ImGui::InputInt("Index##Mgmt", &removeIdx, 1, 1);
    removeIdx = std::max(0, std::min(removeIdx, data.getTrackCount() - 1));
    ImGui::SameLine();
    if (ImGui::Button("Remove##Mgmt")) {
        data.removeTrack(removeIdx);
        if (removeIdx >= data.getTrackCount() && data.getTrackCount() > 0) {
            removeIdx = data.getTrackCount() - 1;
        }
    }

    // 移除所有
    ImGui::SameLine();
    if (ImGui::Button("Remove All##Mgmt")) {
        while (data.getTrackCount() > 0) data.removeTrack(0);
    }

    // 关键帧统计
    ImGui::Separator();
    ImGui::Text("Keyframe Statistics: total=%d", data.getKeyframeCount());
    for (int ti = 0; ti < data.getTrackCount(); ++ti) {
        ImGui::BulletText("Track %d: %zu keyframes",
                          ti, data.tracks_[ti].keyframes.size());
    }
}

// 注册 GUI 测试
REGISTER_GUI_TEST("Timeline", test_gui_timeline_info);
REGISTER_GUI_TEST("Timeline", test_gui_timeline_controls);
REGISTER_GUI_TEST("Timeline", test_gui_keyframe_editor);
REGISTER_GUI_TEST("Timeline", test_gui_track_management);
