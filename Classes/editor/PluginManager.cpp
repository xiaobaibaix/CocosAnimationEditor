// PluginManager.cpp
#include "PluginManager.h"
#include <cstring>
#include "cocos2d.h"

PluginManager& PluginManager::getInstance() {
    static PluginManager instance;
    return instance;
}

void PluginManager::registerPlugin(std::unique_ptr<IEditorPlugin> plugin) {
    if (!plugin) return;
    CCLOG("[PluginManager] registered: %s", plugin->getName());
    plugins_.push_back(std::move(plugin));
}

void PluginManager::initAll() {
    if (initialized_) return;
    for (auto& p : plugins_) {
        CCLOG("[PluginManager] init: %s", p->getName());
        if (!p->onInit()) {
            CCLOG("[PluginManager] WARNING: %s init failed", p->getName());
        }
    }
    initialized_ = true;
    CCLOG("[PluginManager] %zu plugins initialized", plugins_.size());
}

void PluginManager::updateAll(float dt) {
    for (auto& p : plugins_) {
        p->onUpdate(dt);
    }
}

void PluginManager::shutdownAll() {
    for (auto& p : plugins_) {
        CCLOG("[PluginManager] shutdown: %s", p->getName());
        p->onShutdown();
    }
    plugins_.clear();
    initialized_ = false;
}

IEditorPlugin* PluginManager::getPlugin(const char* name) const {
    for (auto& p : plugins_) {
        if (std::strcmp(p->getName(), name) == 0) return p.get();
    }
    return nullptr;
}
