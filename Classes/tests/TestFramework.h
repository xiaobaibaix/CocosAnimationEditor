// TestFramework.h - 动画编辑器测试框架
//
// API 测试：编译运行后在控制台输出 PASS/FAIL
// GUI 测试：在 ImGui 窗口中渲染交互式测试界面
//
// 用法:
//   bool test_my_api() { ... return true; }
//   REGISTER_API_TEST(MyFeature, test_my_api)
//
//   void test_my_gui() { ImGui::Text("Hello"); }
//   REGISTER_GUI_TEST(MyFeature, test_my_gui)
//
//   在插件 onUpdate 中:
//   TestFramework::runApiTests("MyFeature");
//   TestFramework::runGuiTests("MyFeature");
//
//   API 测试在 onInit 中运行一次，GUI 测试在 onUpdate 中显示交互窗口

#ifndef EDITOR_TEST_FRAMEWORK_H
#define EDITOR_TEST_FRAMEWORK_H

#include <string>
#include <vector>
#include <functional>
#include "imgui.h"
#include "cocos2d.h"

struct ApiTest {
    std::string suite;
    std::string name;
    std::function<bool()> func;
};

struct GuiTest {
    std::string suite;
    std::string name;
    std::function<void()> func;
};

class TestFramework {
public:
    static TestFramework& getInstance() {
        static TestFramework instance;
        return instance;
    }

    void registerApiTest(const char* suite, const char* name,
                         std::function<bool()> func) {
        apiTests_.push_back({suite, name, std::move(func)});
    }

    void registerGuiTest(const char* suite, const char* name,
                         std::function<void()> func) {
        guiTests_.push_back({suite, name, std::move(func)});
    }

    /// 运行指定 suite 的所有 API 测试，返回失败数
    int runApiTests(const char* suite) {
        int passed = 0, failed = 0;
        for (auto& t : apiTests_) {
            if (t.suite != suite) continue;
            bool ok = false;
            try { ok = t.func(); } catch (...) {}
            CCLOG("[API] %s::%s %s", suite, t.name.c_str(),
                  ok ? "PASS" : "FAIL");
            ok ? ++passed : ++failed;
        }
        CCLOG("[API] %s: %d passed, %d failed", suite, passed, failed);
        return failed;
    }

    /// 在 ImGui 窗口中显示指定 suite 的 GUI 测试
    void runGuiTests(const char* suite) {
        if (!ImGui::Begin("GUI Test Runner")) return;
        for (auto& t : guiTests_) {
            if (t.suite != suite) continue;
            ImGui::SeparatorText(t.name.c_str());
            t.func();
        }
        ImGui::End();
    }

private:
    std::vector<ApiTest> apiTests_;
    std::vector<GuiTest> guiTests_;
};

// 注册宏
#define REGISTER_API_TEST(suite, func) \
    static bool _api_##func = []() { \
        TestFramework::getInstance().registerApiTest(suite, #func, func); \
        return true; \
    }()

#define REGISTER_GUI_TEST(suite, func) \
    static bool _gui_##func = []() { \
        TestFramework::getInstance().registerGuiTest(suite, #func, func); \
        return true; \
    }()

#endif
