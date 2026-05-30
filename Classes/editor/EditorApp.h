// EditorApp.h - 动画编辑器主场景（UGF 框架）
#ifndef EDITOR_APP_H
#define EDITOR_APP_H

#include "cocos2d.h"
#include "UGF.hpp"
#include "PluginSystem.hpp"

class EditorApp : public cocos2d::Scene {
public:
    static cocos2d::Scene* createScene();
    virtual bool init() override;
    virtual void onEnter() override;
    virtual void onExit() override;
    virtual void visit(cocos2d::Renderer*, const cocos2d::Mat4&, uint32_t) override;
    CREATE_FUNC(EditorApp);
private:
    void onEditorUpdate(float dt);
    void setupImGuiBackends();
    void shutdownImGuiBackends();
    void registerBuiltinPlugins();
    GLFWwindow* glfwWindow_ = nullptr;
    bool imguiBackendsReady_ = false;
    ugf::PluginSystem pluginSystem_;
};

#endif
