// PluginManager.h - UGF 事件总线驱动的插件管理器
#ifndef EDITOR_PLUGIN_MANAGER_H
#define EDITOR_PLUGIN_MANAGER_H

#include "PluginInterface.h"
#include <vector>
#include <memory>

class PluginManager {
public:
    static PluginManager& getInstance();

    /// 注册插件（接管所有权）
    void registerPlugin(std::unique_ptr<IEditorPlugin> plugin);

    /// 初始化所有已注册的插件
    void initAll();

    /// 每帧更新所有插件
    void updateAll(float dt);

    /// 关闭并清空所有插件
    void shutdownAll();

    /// 按名称获取插件
    IEditorPlugin* getPlugin(const char* name) const;

    /// 已注册的插件数量
    size_t getPluginCount() const { return plugins_.size(); }

private:
    PluginManager() = default;
    std::vector<std::unique_ptr<IEditorPlugin>> plugins_;
    bool initialized_ = false;
};

#endif // EDITOR_PLUGIN_MANAGER_H
