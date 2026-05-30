// EditorApp.h - 动画编辑器主场景（取代 HelloWorldScene）
// 负责 ImGui 渲染循环和插件生命周期管理
#ifndef EDITOR_APP_H
#define EDITOR_APP_H

#include "cocos2d.h"
#include "PluginManager.h"

class EditorApp : public cocos2d::Scene {
public:
    static cocos2d::Scene* createScene();

    virtual bool init() override;
    virtual void onEnter() override;
    virtual void onExit() override;
    virtual void visit(cocos2d::Renderer* renderer,
                       const cocos2d::Mat4& transform,
                       uint32_t parentFlags) override;

    CREATE_FUNC(EditorApp);

private:
    void onEditorUpdate(float dt);
    void setupImGuiBackends();
    void shutdownImGuiBackends();
    void registerBuiltinPlugins();

    GLFWwindow* glfwWindow_ = nullptr;
    bool imguiBackendsReady_ = false;
};

#endif // EDITOR_APP_H
