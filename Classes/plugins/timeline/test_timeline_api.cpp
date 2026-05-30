// test_timeline_api.cpp - TimelineData 组件 API 测试
#include "TimelinePlugin.h"
#include "tests/TestFramework.h"
#include "imgui.h"
#include "cocos2d.h"

// ================================================================
//  API 测试 — TimelineData 组件功能验证
// ================================================================

static bool test_timeline_create() {
    TimelineData data;
    bool ok = data.initialize();
    if (!ok) return false;
    if (data.getComponentId() != "TimelineData") return false;
    if (data.totalFrames != 60) return false;
    if (data.currentFrame != 0) return false;
    if (data.isPlaying) return false;   // 默认暂停
    if (data.fps != 30) return false;   // 默认 30 fps
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

    // 初始状态：暂停
    if (data.isPlaying) return false;

    // 播放
    data.play();
    if (!data.isPlaying) return false;

    // 暂停
    data.pause();
    if (data.isPlaying) return false;

    data.terminate();
    return true;
}

static bool test_timeline_advance() {
    TimelineData data;
    data.initialize();

    // 从 0 前进一帧
    data.advance();
    if (data.currentFrame != 1) return false;

    // 连续前进
    for (int i = 0; i < 9; i++) data.advance();
    if (data.currentFrame != 10) return false;

    // 前进到 totalFrames 然后循环
    data.seekTo(data.totalFrames - 1);  // 59
    data.advance();  // 60
    if (data.currentFrame != 60) return false;
    data.advance();  // 循环回 0
    if (data.currentFrame != 0) return false;

    data.terminate();
    return true;
}

static bool test_timeline_fps() {
    TimelineData data;
    data.initialize();

    if (data.fps != 30) return false;

    // 可修改 fps
    data.fps = 60;
    if (data.fps != 60) return false;

    data.terminate();
    return true;
}

// 注册 API 测试
REGISTER_API_TEST("Timeline", test_timeline_create);
REGISTER_API_TEST("Timeline", test_timeline_seek);
REGISTER_API_TEST("Timeline", test_timeline_play_pause);
REGISTER_API_TEST("Timeline", test_timeline_advance);
REGISTER_API_TEST("Timeline", test_timeline_fps);

// ================================================================
//  GUI 测试 — 在 ImGui 窗口中渲染测试信息
// ================================================================

static void test_gui_timeline_info() {
    ImGui::Text("TimelineData Component Test");
    ImGui::Text("  totalFrames: 60 (default)");
    ImGui::Text("  fps: 30 (default)");
    ImGui::Text("  getComponentId(): TimelineData");
    ImGui::Text("  Controls: play(), pause(), seekTo(n), advance()");

    // 实时创建并测试
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

    if (ImGui::Button("Play##GuiTest")) {
        testData.play();
    }
    ImGui::SameLine();
    if (ImGui::Button("Pause##GuiTest")) {
        testData.pause();
    }
    ImGui::SameLine();
    if (ImGui::Button("Advance##GuiTest")) {
        testData.advance();
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset##GuiTest")) {
        testData.seekTo(0);
    }

    if (testData.isPlaying) {
        testData.advance();
    }

    // 进度条显示
    float progress = (float)testData.currentFrame / (float)testData.totalFrames;
    ImGui::ProgressBar(progress, ImVec2(-1.0f, 0.0f));
}

REGISTER_GUI_TEST(Timeline, test_gui_timeline_info);
