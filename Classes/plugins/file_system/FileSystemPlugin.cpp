// FileSystemPlugin.cpp - File browser / project management plugin
#include "plugins/file_system/FileSystemPlugin.h"
#include "editor/EditorEvents.h"
#include "cocos2d.h"
#include <dirent.h>
#include <sys/stat.h>
#include <cstring>
#include <algorithm>
#include <sstream>
#include <fstream>
#include <cerrno>

// ============================================================
//  JSON Helpers (simple string concatenation, no external lib)
// ============================================================

static std::string escapeJson(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 2);
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:   out += c;      break;
        }
    }
    return out;
}

// Extract string value between quotes after a given key in JSON text
// E.g., findJsonString(json, "\"name\"") returns the value of "name"
static std::string findJsonString(const std::string& json, const std::string& key) {
    auto pos = json.find(key);
    if (pos == std::string::npos) return "";
    pos = json.find('"', pos + key.size());
    if (pos == std::string::npos) return "";
    auto end = json.find('"', pos + 1);
    if (end == std::string::npos) return "";
    return json.substr(pos + 1, end - pos - 1);
}

// Extract a numeric value after a given key
static float findJsonNumber(const std::string& json, const std::string& key) {
    auto pos = json.find(key);
    if (pos == std::string::npos) return 0.0f;
    pos = json.find(':', pos + key.size());
    if (pos == std::string::npos) return 0.0f;
    // Skip whitespace and colon
    pos++;
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t' || json[pos] == '\n')) pos++;
    // Parse number
    char* end = nullptr;
    float val = std::strtof(json.c_str() + pos, &end);
    return val;
}

// Find the start position of the n-th occurrence of a substring
static size_t findNth(const std::string& s, const std::string& sub, int n) {
    size_t pos = 0;
    for (int i = 0; i < n; ++i) {
        pos = s.find(sub, pos);
        if (pos == std::string::npos) return std::string::npos;
        if (i < n - 1) pos += sub.size();
    }
    return pos;
}

// Generate a complete scene JSON string
static std::string toJson(const std::string& sceneName,
                          const std::string& nodesJson,
                          const std::string& animationsJson) {
    std::string json;
    json += "{\n";
    json += "  \"version\": \"1.0\",\n";
    json += "  \"name\": \"" + escapeJson(sceneName) + "\",\n";
    json += "  \"nodes\": [\n" + nodesJson + "\n  ],\n";
    json += "  \"animations\": [\n" + animationsJson + "\n  ]\n";
    json += "}\n";
    return json;
}

// ============================================================
//  FSModel : ugf::Component
// ============================================================

bool FSModel::initialize(const std::unordered_map<std::string, std::any>&) {
    sceneDirty_ = false;
    currentSceneName_ = "untitled";

    // Subscribe to SceneDataChangedEvent to track dirty state
    sceneDirtyConn_ = ugf::EventBus::getInstance().subscribe<SceneDataChangedEvent>(
        [this](const SceneDataChangedEvent&) {
            sceneDirty_ = true;
        });

    CCLOG("[FSModel] Initialized");
    return true;
}

void FSModel::terminate() {
    sceneDirtyConn_.release();
    entries_.clear();
    projectPath_.clear();
    projectName_.clear();
    CCLOG("[FSModel] Terminated");
}

void FSModel::setProjectPath(const std::string& path) {
    projectPath_ = path;

    // Extract project name from path (last component)
    size_t lastSep = path.rfind('/');
    if (lastSep == std::string::npos) {
        projectName_ = path;
    } else {
        projectName_ = path.substr(lastSep + 1);
    }

    CCLOG("[FSModel] Project set: %s (%s)", projectName_.c_str(), projectPath_.c_str());

    // Scan the project directory
    scanDirectory(projectPath_);

    // Notify other plugins
    ugf::EventBus::getInstance().publish(ProjectOpenedEvent{projectPath_, projectName_});
    ugf::EventBus::getInstance().publish(ProjectLoadedEvent{projectPath_, projectName_});
}

void FSModel::scanDirectory(const std::string& dir) {
    entries_.clear();

    DIR* dp = opendir(dir.c_str());
    if (!dp) {
        CCLOG("[FSModel] Failed to open directory: %s (errno=%d)", dir.c_str(), errno);
        return;
    }

    struct dirent* entry;
    while ((entry = readdir(dp)) != nullptr) {
        std::string name = entry->d_name;
        if (name == "." || name == "..") continue;

        FSEntry fe;
        fe.name = name;
        fe.path = name; // relative to project root

        // Determine if it's a directory
        if (entry->d_type == DT_DIR) {
            fe.isDir = true;
        } else if (entry->d_type == DT_UNKNOWN) {
            // Fallback: use stat()
            std::string fullPath = dir + "/" + name;
            struct stat st;
            fe.isDir = (stat(fullPath.c_str(), &st) == 0 && S_ISDIR(st.st_mode));
        } else {
            fe.isDir = false;
        }

        // Filter for relevant files (.anim, .json, .png, .jpg, .bmp, .tga)
        if (!fe.isDir) {
            std::string ext;
            auto dotPos = name.rfind('.');
            if (dotPos != std::string::npos) {
                ext = name.substr(dotPos);
            }
            // Also allow directories and common asset types
            if (ext != ".anim" && ext != ".json" && ext != ".png" &&
                ext != ".jpg" && ext != ".jpeg" && ext != ".bmp" &&
                ext != ".tga" && ext != ".csb" && ext != ".plist") {
                continue; // skip irrelevant files
            }
        }

        entries_.push_back(fe);
    }
    closedir(dp);

    // Sort: directories first, then alphabetical
    std::sort(entries_.begin(), entries_.end(),
              [](const FSEntry& a, const FSEntry& b) {
                  if (a.isDir != b.isDir) return a.isDir > b.isDir;
                  return a.name < b.name;
              });

    CCLOG("[FSModel] Scanned %s: %zu entries", dir.c_str(), entries_.size());
}

std::string FSModel::serializeSceneToJson() {
    // Generate JSON with the current scene structure.
    // For now, we produce a template scene with a root node and a default animation.
    std::string nodesJson =
        "    {\"name\": \"SceneRoot\", \"children\": [\"Sprite_Hero\", \"Bone_Arm\"], "
        "\"properties\": {\"x\": 0, \"y\": 0}},\n"
        "    {\"name\": \"Sprite_Hero\", \"parent\": \"SceneRoot\", "
        "\"properties\": {\"x\": 100, \"y\": 200}},\n"
        "    {\"name\": \"Bone_Arm\", \"parent\": \"SceneRoot\", "
        "\"properties\": {\"x\": 50, \"y\": 100}}";

    std::string animationsJson =
        "    {\"name\": \"idle\", \"targetNode\": \"Sprite_Hero\", \"fps\": 30, "
        "\"totalFrames\": 60,\n"
        "     \"tracks\": [\n"
        "       {\"property\": \"position.x\", \"keyframes\": ["
        "{\"frame\":0,\"value\":0}, {\"frame\":60,\"value\":100}]},\n"
        "       {\"property\": \"position.y\", \"keyframes\": ["
        "{\"frame\":0,\"value\":0}, {\"frame\":60,\"value\":50}]}\n"
        "     ]}";

    currentSceneJson_ = toJson(currentSceneName_, nodesJson, animationsJson);
    sceneDirty_ = false;

    CCLOG("[FSModel] Serialized scene '%s' to JSON (%zu chars)",
          currentSceneName_.c_str(), currentSceneJson_.size());
    return currentSceneJson_;
}

bool FSModel::loadSceneFromJson(const std::string& json) {
    if (json.empty()) {
        CCLOG("[FSModel] loadSceneFromJson: empty JSON");
        return false;
    }

    currentSceneJson_ = json;
    sceneDirty_ = false;

    // Parse scene name
    std::string sceneName = findJsonString(json, "\"name\"");
    if (!sceneName.empty()) {
        currentSceneName_ = sceneName;
    }
    CCLOG("[FSModel] Loading scene: %s", currentSceneName_.c_str());

    // Parse nodes: find all occurrences of "{\"name\":"
    size_t searchPos = 0;
    const std::string nodeMarker = "{\"name\":";
    while (true) {
        size_t nodeStart = json.find(nodeMarker, searchPos);
        if (nodeStart == std::string::npos) break;

        // Extract node name from this object
        size_t nameValStart = json.find('"', nodeStart + nodeMarker.size());
        if (nameValStart == std::string::npos) break;
        size_t nameValEnd = json.find('"', nameValStart + 1);
        if (nameValEnd == std::string::npos) break;
        std::string nodeName = json.substr(nameValStart + 1, nameValEnd - nameValStart - 1);

        // Extract parent name if present
        std::string parentName;
        size_t parentMarker = json.find("\"parent\":", nameValEnd);
        if (parentMarker != std::string::npos && parentMarker < json.find('}', nameValEnd)) {
            size_t pValStart = json.find('"', parentMarker + 10);
            if (pValStart != std::string::npos) {
                size_t pValEnd = json.find('"', pValStart + 1);
                if (pValEnd != std::string::npos) {
                    parentName = json.substr(pValStart + 1, pValEnd - pValStart - 1);
                }
            }
        }

        // Publish NodeAddedEvent for each parsed node (skip root-like nodes for parenting)
        if (nodeName != "SceneRoot" && !parentName.empty()) {
            ugf::EventBus::getInstance().publish(NodeAddedEvent{nodeName, parentName});
            CCLOG("[FSModel] Published NodeAddedEvent: %s (parent: %s)",
                  nodeName.c_str(), parentName.c_str());
        }

        // Move past this node object
        searchPos = json.find('}', nameValEnd);
        if (searchPos == std::string::npos) break;
        searchPos++;
    }

    // Publish scene loaded event
    ugf::EventBus::getInstance().publish(SceneLoadedEvent{currentSceneName_});

    CCLOG("[FSModel] Scene loaded: %s", currentSceneName_.c_str());
    return true;
}

// ============================================================
//  FSView : ugf::Component
// ============================================================

bool FSView::initialize(const std::unordered_map<std::string, std::any>& config) {
    auto itModel = config.find("model");
    if (itModel != config.end()) {
        model_ = std::any_cast<FSModel*>(itModel->second);
    }
    CCLOG("[FSView] Initialized (model=%p)", static_cast<void*>(model_));
    return true;
}

void FSView::terminate() {
    model_ = nullptr;
    CCLOG("[FSView] Terminated");
}

void FSView::update(float) {
    if (!windowOpen_) return;

    ImGui::SetNextWindowSize(ImVec2(300, 400), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("File System", &windowOpen_)) {
        ImGui::End();
        return;
    }

    renderToolbar();
    ImGui::Separator();
    renderFileTree();

    // Render popups
    renderNewProjectPopup();
    renderNewScenePopup();

    ImGui::End();
}

void FSView::renderToolbar() {
    // "New Project" button
    if (ImGui::Button("New Project")) {
        showNewProject_ = true;
        projectNameBuf_[0] = '\0';
        projectPathBuf_[0] = '\0';
    }
    ImGui::SameLine();

    // "New Scene" button (only active if a project is open)
    if (ImGui::Button("New Scene")) {
        showNewScene_ = true;
        sceneNameBuf_[0] = '\0';
    }
    ImGui::SameLine();

    // "Save Scene" button
    if (ImGui::Button("Save Scene")) {
        if (model_ && !model_->getProjectPath().empty()) {
            std::string json = model_->serializeSceneToJson();
            std::string filePath = model_->getProjectPath() + "/" +
                                   model_->getProjectName() + ".anim";
            std::ofstream out(filePath);
            if (out.is_open()) {
                out << json;
                out.close();
                CCLOG("[FSView] Scene saved to: %s", filePath.c_str());
                ugf::EventBus::getInstance().publish(SceneSavedEvent{filePath});
            } else {
                CCLOG("[FSView] Failed to save scene to: %s", filePath.c_str());
            }
        } else {
            CCLOG("[FSView] No project open — cannot save scene");
        }
    }

    // Project name display
    if (model_ && !model_->getProjectName().empty()) {
        ImGui::SameLine();
        std::string label = "Project: " + model_->getProjectName();
        if (model_->isSceneDirty()) {
            label += " *";
        }
        ImGui::Text("%s", label.c_str());
    }
}

void FSView::renderFileTree() {
    if (!model_) {
        ImGui::TextDisabled("No project loaded.");
        return;
    }

    if (model_->entries().empty()) {
        ImGui::TextDisabled("(empty directory)");
        return;
    }

    for (const auto& entry : model_->entries()) {
        renderEntry(entry);
    }
}

void FSView::renderEntry(const FSEntry& entry) {
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow;
    if (!entry.isDir) {
        flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    }

    // Choose icon based on file type
    const char* icon = "   "; // fallback
    if (entry.isDir) {
        icon = " D "; // directory icon placeholder
    } else {
        auto dotPos = entry.name.rfind('.');
        if (dotPos != std::string::npos) {
            std::string ext = entry.name.substr(dotPos);
            if (ext == ".anim") icon = " A "; // animation icon placeholder
            else if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp")
                icon = " I "; // image icon placeholder
        }
    }

    std::string label = std::string(icon) + entry.name;
    bool opened = ImGui::TreeNodeEx(label.c_str(), flags);

    // Double-click to open .anim files
    if (!entry.isDir && ImGui::IsItemHovered() &&
        ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        auto dotPos = entry.name.rfind('.');
        if (dotPos != std::string::npos) {
            std::string ext = entry.name.substr(dotPos);
            if (ext == ".anim" && model_) {
                CCLOG("[FSView] Double-clicked: %s", entry.name.c_str());
                std::string fullPath = model_->getProjectPath() + "/" + entry.path;
                // Read file and load scene
                std::ifstream in(fullPath);
                if (in.is_open()) {
                    std::stringstream buffer;
                    buffer << in.rdbuf();
                    in.close();
                    model_->loadSceneFromJson(buffer.str());
                    ugf::EventBus::getInstance().publish(
                        FileOpenedEvent{entry.path, entry.name});
                    CCLOG("[FSView] File opened: %s", entry.name.c_str());
                }
            }
        }
    }

    // Right-click context menu
    if (ImGui::BeginPopupContextItem()) {
        if (!entry.isDir) {
            if (ImGui::MenuItem("Open")) {
                if (model_) {
                    std::string fullPath = model_->getProjectPath() + "/" + entry.path;
                    std::ifstream in(fullPath);
                    if (in.is_open()) {
                        std::stringstream buffer;
                        buffer << in.rdbuf();
                        in.close();
                        model_->loadSceneFromJson(buffer.str());
                        ugf::EventBus::getInstance().publish(
                            FileOpenedEvent{entry.path, entry.name});
                    }
                }
            }
        }
        if (ImGui::MenuItem("Delete")) {
            if (model_) {
                std::string fullPath = model_->getProjectPath() + "/" + entry.path;
                if (entry.isDir) {
                    rmdir(fullPath.c_str());
                } else {
                    remove(fullPath.c_str());
                }
                CCLOG("[FSView] Deleted: %s", entry.path.c_str());
                // Re-scan directory
                model_->scanDirectory(model_->getProjectPath());
            }
        }
        ImGui::EndPopup();
    }

    // For directories, recurse (placeholder: directories are leaf for now since
    // scanDirectory only lists direct children)
    if (entry.isDir && opened) {
        ImGui::TreePop();
    }
}

void FSView::renderNewProjectPopup() {
    if (!showNewProject_) return;

    ImGui::OpenPopup("New Project");
    ImGui::SetNextWindowSize(ImVec2(420, 200), ImGuiCond_Appearing);
    if (ImGui::BeginPopupModal("New Project", &showNewProject_,
                                ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Create a new project folder:");

        ImGui::SetNextItemWidth(380);
        ImGui::InputTextWithHint("Project Name", "e.g. MyAnimation",
                                  projectNameBuf_, sizeof(projectNameBuf_));

        ImGui::SetNextItemWidth(380);
        ImGui::InputTextWithHint("Project Path", "e.g. /home/user/projects",
                                  projectPathBuf_, sizeof(projectPathBuf_));

        ImGui::Spacing();

        bool canCreate = (std::strlen(projectNameBuf_) > 0 &&
                          std::strlen(projectPathBuf_) > 0);

        if (!canCreate) ImGui::BeginDisabled();
        if (ImGui::Button("Create", ImVec2(120, 0))) {
            // Build full project directory path
            std::string fullPath = std::string(projectPathBuf_) + "/" +
                                   std::string(projectNameBuf_);

            // Create directory (with mkdir)
            if (mkdir(fullPath.c_str(), 0755) == 0 || errno == EEXIST) {
                CCLOG("[FSView] Created project directory: %s", fullPath.c_str());
                if (model_) {
                    model_->setProjectPath(fullPath);
                }
                showNewProject_ = false;
                ImGui::CloseCurrentPopup();
            } else {
                CCLOG("[FSView] Failed to create project directory: %s (errno=%d)",
                      fullPath.c_str(), errno);
            }
        }
        if (!canCreate) ImGui::EndDisabled();

        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            showNewProject_ = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

void FSView::renderNewScenePopup() {
    if (!showNewScene_) return;

    ImGui::OpenPopup("New Scene");
    ImGui::SetNextWindowSize(ImVec2(400, 150), ImGuiCond_Appearing);
    if (ImGui::BeginPopupModal("New Scene", &showNewScene_,
                                ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Create a new animation scene:");

        ImGui::SetNextItemWidth(380);
        ImGui::InputTextWithHint("Scene Name", "e.g. HeroIdle",
                                  sceneNameBuf_, sizeof(sceneNameBuf_));

        ImGui::Spacing();

        bool canCreate = (std::strlen(sceneNameBuf_) > 0 && model_ &&
                          !model_->getProjectPath().empty());

        if (!canCreate) ImGui::BeginDisabled();
        if (ImGui::Button("Create", ImVec2(120, 0))) {
            std::string sceneName(sceneNameBuf_);
            std::string filePath = model_->getProjectPath() + "/" + sceneName + ".anim";

            // Generate a default empty scene JSON
            std::string emptyNodes = "    {\"name\": \"SceneRoot\", \"children\": [], "
                                     "\"properties\": {\"x\": 0, \"y\": 0}}";
            std::string emptyAnims = "";
            std::string json = toJson(sceneName, emptyNodes, emptyAnims);

            std::ofstream out(filePath);
            if (out.is_open()) {
                out << json;
                out.close();
                CCLOG("[FSView] Created scene file: %s", filePath.c_str());
                // Re-scan directory
                model_->scanDirectory(model_->getProjectPath());
                showNewScene_ = false;
                ImGui::CloseCurrentPopup();
            } else {
                CCLOG("[FSView] Failed to create scene file: %s", filePath.c_str());
            }
        }
        if (!canCreate) ImGui::EndDisabled();

        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            showNewScene_ = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

// ============================================================
//  FileSystemPlugin : ugf::IPlugin
// ============================================================

bool FileSystemPlugin::initialize() {
    CCLOG("[FileSystemPlugin] Initializing...");

    // 1) Register FSModel
    auto* model = componentSystem.registerComponent<FSModel>("model");
    if (!model) {
        CCLOG("[FileSystemPlugin] Failed to register FSModel");
        return false;
    }

    // 2) Register FSView, passing the model
    std::unordered_map<std::string, std::any> viewCfg;
    viewCfg["model"] = model;
    auto* view = componentSystem.registerComponent<FSView>("view", viewCfg);
    if (!view) {
        CCLOG("[FileSystemPlugin] Failed to register FSView");
        return false;
    }

    CCLOG("[FileSystemPlugin] Initialized — %zu components",
          componentSystem.size());
    return true;
}

void FileSystemPlugin::update(float dt) {
    componentSystem.updateAll(dt);
}

void FileSystemPlugin::shutdown() {
    CCLOG("[FileSystemPlugin] Shutting down.");
    componentSystem.clear();
}
