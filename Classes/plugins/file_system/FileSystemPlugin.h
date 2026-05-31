// FileSystemPlugin.h - File browser / project management plugin
#ifndef FILESYSTEM_PLUGIN_H
#define FILESYSTEM_PLUGIN_H

#include "PluginSystem.hpp"
#include "ComponentSystem.hpp"
#include "EventSystem.hpp"
#include "imgui.h"
#include <string>
#include <vector>
#include <functional>
#include <unordered_map>
#include <any>

// ============================================================
//  FSEntry - File entry for browser
// ============================================================
struct FSEntry {
    std::string name;
    std::string path;      // relative to project root
    bool isDir = false;
};

// ============================================================
//  FSEvents - Cross-module communication
// ============================================================
struct ProjectOpenedEvent { std::string projectPath; std::string projectName; };
struct FileOpenedEvent { std::string filePath; std::string fileName; };
struct SceneSavedEvent { std::string filePath; };
struct SceneLoadedEvent { std::string filePath; };

// ============================================================
//  FSModel : ugf::Component — Data component (project state)
// ============================================================
class FSModel : public ugf::Component {
public:
    std::string getComponentId() const override { return "FileSystem.Model"; }
    bool initialize(const std::unordered_map<std::string, std::any>& config = {}) override;
    void update(float) override {}
    void terminate() override;

    void setProjectPath(const std::string& path);
    std::string getProjectPath() const { return projectPath_; }
    std::string getProjectName() const { return projectName_; }
    void scanDirectory(const std::string& dir);
    const std::vector<FSEntry>& entries() const { return entries_; }

    // Serialization
    std::string serializeSceneToJson();  // serializes current scene to JSON
    bool loadSceneFromJson(const std::string& json);

    // Dirty state tracking
    bool isSceneDirty() const { return sceneDirty_; }
    void clearDirty() { sceneDirty_ = false; }

private:
    std::string projectPath_;
    std::string projectName_;
    std::vector<FSEntry> entries_;

    // Scene serialization state
    std::string currentSceneJson_;
    std::string currentSceneName_;
    bool sceneDirty_ = false;
    ugf::EventConnection sceneDirtyConn_;
};

// ============================================================
//  FSView : ugf::Component — UI component (file browser)
// ============================================================
class FSView : public ugf::Component {
public:
    std::string getComponentId() const override { return "FileSystem.View"; }
    bool initialize(const std::unordered_map<std::string, std::any>& config = {}) override;
    void update(float) override;
    void terminate() override;

private:
    void renderToolbar();
    void renderFileTree();
    void renderEntry(const FSEntry& entry);
    void renderNewProjectPopup();
    void renderNewScenePopup();

    FSModel* model_ = nullptr;
    bool windowOpen_ = true;

    // Popup state
    bool showNewProject_ = false;
    bool showNewScene_ = false;
    char projectNameBuf_[256] = {};
    char sceneNameBuf_[256] = {};
    char projectPathBuf_[512] = {};
};

// ============================================================
//  FileSystemPlugin : ugf::IPlugin — Plugin entry
// ============================================================
class FileSystemPlugin : public ugf::IPlugin {
public:
    std::string getId() const override { return "FileSystem"; }
    bool initialize() override;
    void update(float dt) override;
    void shutdown() override;
};

#endif // FILESYSTEM_PLUGIN_H
