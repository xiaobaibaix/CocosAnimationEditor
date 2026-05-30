// PluginInterface.h - 动画编辑器插件基类
// 所有 UI 插件（场景树、属性编辑器、时间轴、行为树）实现此接口
#ifndef EDITOR_PLUGIN_INTERFACE_H
#define EDITOR_PLUGIN_INTERFACE_H

class IEditorPlugin {
public:
    virtual ~IEditorPlugin() = default;

    /// 插件唯一名称，如 "SceneTree", "Timeline"
    virtual const char* getName() const = 0;

    /// 插件初始化（注册时调用一次）
    virtual bool onInit() = 0;

    /// 每帧更新，在这里写 ImGui 绘制代码
    virtual void onUpdate(float dt) = 0;

    /// 插件卸载
    virtual void onShutdown() = 0;
};

#endif // EDITOR_PLUGIN_INTERFACE_H
