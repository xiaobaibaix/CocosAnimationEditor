// PropertyEditorPlugin.cpp - Property Editor Plugin implementation
//
// Three UGF Components:
//   1. PropertyBag        — data store (default: Name="Hero", X=100, Y=200, Rotation=0.0)
//   2. PropertyEditorView — ImGui "Property Editor" window with Property|Value table
//   3. PropertyEventHandler — listens to NodeSelectedEvent, updates bag
//
// Plugin initialize() wires them together through ComponentSystem.

#include "PropertyEditorPlugin.h"
#include "editor/EditorEvents.h"
#include "tests/TestFramework.h"
#include "cocos2d.h"
#include "imgui.h"
#include <cstring>
#include <sstream>

// ================================================================
// PropertyBag
// ================================================================

bool PropertyBag::initialize(const std::unordered_map<std::string, std::any>&) {
    // Set default properties
    setString("Name", "Hero");
    setInt("X", 100);
    setInt("Y", 200);
    setFloat("Rotation", 0.0f);
    return true;
}

void PropertyBag::update(float) {
    // Data-only component; no per-frame logic.
}

void PropertyBag::terminate() {
    properties_.clear();
}

void PropertyBag::setInt(const std::string& key, int value) {
    auto& pv = properties_[key];
    pv.type = StoredType::Int;
    pv.intVal = value;
}

void PropertyBag::setFloat(const std::string& key, float value) {
    auto& pv = properties_[key];
    pv.type = StoredType::Float;
    pv.floatVal = value;
}

void PropertyBag::setString(const std::string& key, const std::string& value) {
    auto& pv = properties_[key];
    pv.type = StoredType::String;
    pv.stringVal = value;
}

int PropertyBag::getInt(const std::string& key, int defaultVal) const {
    auto it = properties_.find(key);
    if (it != properties_.end() && it->second.type == StoredType::Int) {
        return it->second.intVal;
    }
    return defaultVal;
}

float PropertyBag::getFloat(const std::string& key, float defaultVal) const {
    auto it = properties_.find(key);
    if (it != properties_.end() && it->second.type == StoredType::Float) {
        return it->second.floatVal;
    }
    return defaultVal;
}

std::string PropertyBag::getString(const std::string& key, const std::string& defaultVal) const {
    auto it = properties_.find(key);
    if (it != properties_.end() && it->second.type == StoredType::String) {
        return it->second.stringVal;
    }
    return defaultVal;
}

bool PropertyBag::has(const std::string& key) const {
    return properties_.find(key) != properties_.end();
}

std::vector<std::string> PropertyBag::getAllKeys() const {
    std::vector<std::string> keys;
    for (const auto& [key, _] : properties_) {
        keys.push_back(key);
    }
    return keys;
}

std::string PropertyBag::getAsString(const std::string& key) const {
    auto it = properties_.find(key);
    if (it == properties_.end()) return "";
    switch (it->second.type) {
        case StoredType::Int:
            return std::to_string(it->second.intVal);
        case StoredType::Float:
            return std::to_string(it->second.floatVal);
        case StoredType::String:
            return it->second.stringVal;
    }
    return "";
}

PropertyBag::ValueType PropertyBag::getType(const std::string& key) const {
    auto it = properties_.find(key);
    if (it == properties_.end()) return ValueType::Unknown;
    switch (it->second.type) {
        case StoredType::Int:    return ValueType::Int;
        case StoredType::Float:  return ValueType::Float;
        case StoredType::String: return ValueType::String;
    }
    return ValueType::Unknown;
}

// ================================================================
// PropertyEditorView
// ================================================================

bool PropertyEditorView::initialize(const std::unordered_map<std::string, std::any>& config) {
    auto it = config.find("bag");
    if (it != config.end()) {
        bag_ = std::any_cast<PropertyBag*>(it->second);
    }
    return bag_ != nullptr;
}

void PropertyEditorView::update(float) {
    if (!showWindow_ || !bag_) return;

    ImGui::Begin("Property Editor", &showWindow_,
                 ImGuiWindowFlags_NoDocking);

    renderPropertyTable();

    ImGui::End();

    // Run registered GUI tests
    TestFramework::getInstance().runGuiTests("PropertyEditor");
}

void PropertyEditorView::terminate() {
    bag_ = nullptr;
}

void PropertyEditorView::renderPropertyTable() {
    if (!bag_) return;

    if (!ImGui::BeginTable("##propTable", 2,
                           ImGuiTableFlags_Borders |
                           ImGuiTableFlags_Resizable |
                           ImGuiTableFlags_RowBg)) {
        return;
    }

    ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 100.0f);
    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableHeadersRow();

    auto keys = bag_->getAllKeys();
    for (const auto& key : keys) {
        ImGui::PushID(key.c_str());

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextUnformatted(key.c_str());

        ImGui::TableSetColumnIndex(1);
        ImGui::SetNextItemWidth(-FLT_MIN);

        auto type = bag_->getType(key);
        std::string oldValStr = bag_->getAsString(key);
        bool changed = false;
        std::string newValStr;

        switch (type) {
            case PropertyBag::ValueType::Int: {
                int val = bag_->getInt(key);
                if (ImGui::InputInt("##val", &val)) {
                    bag_->setInt(key, val);
                    changed = true;
                    newValStr = std::to_string(val);
                }
                break;
            }
            case PropertyBag::ValueType::Float: {
                float val = bag_->getFloat(key);
                if (ImGui::InputFloat("##val", &val)) {
                    bag_->setFloat(key, val);
                    changed = true;
                    newValStr = std::to_string(val);
                }
                break;
            }
            case PropertyBag::ValueType::String: {
                char buf[256];
                std::strncpy(buf, bag_->getString(key).c_str(), sizeof(buf) - 1);
                buf[sizeof(buf) - 1] = '\0';
                if (ImGui::InputText("##val", buf, sizeof(buf))) {
                    bag_->setString(key, buf);
                    changed = true;
                    newValStr = buf;
                }
                break;
            }
            default:
                ImGui::TextUnformatted("?");
                break;
        }

        if (changed) {
            CCLOG("[PropertyEditor] '%s': %s -> %s",
                  key.c_str(), oldValStr.c_str(), newValStr.c_str());

            PropertyChangedEvent evt;
            evt.key = key;
            evt.oldValue = oldValStr;
            evt.newValue = newValStr;
            ugf::EventBus::getInstance().publish(evt);
        }

        ImGui::PopID();
    }

    ImGui::EndTable();
}

// ================================================================
// PropertyEventHandler
// ================================================================

bool PropertyEventHandler::initialize(const std::unordered_map<std::string, std::any>& config) {
    auto it = config.find("bag");
    if (it != config.end()) {
        bag_ = std::any_cast<PropertyBag*>(it->second);
    }

    // Subscribe to NodeSelectedEvent
    nodeSelectedConn_ = ugf::EventBus::getInstance().subscribe<NodeSelectedEvent>(
        [this](const NodeSelectedEvent& event) {
            onNodeSelected(event);
        });

    return true;
}

void PropertyEventHandler::update(float) {
    // Event-driven component; no per-frame logic.
}

void PropertyEventHandler::terminate() {
    nodeSelectedConn_.release();
    bag_ = nullptr;
}

void PropertyEventHandler::onNodeSelected(const NodeSelectedEvent& event) {
    CCLOG("[PropertyEditor] NodeSelected: %s", event.nodeName.c_str());

    if (bag_) {
        bag_->setString("Name", event.nodeName);
    }
}

// ================================================================
// PropertyEditorPlugin — wires components together
// ================================================================

bool PropertyEditorPlugin::initialize() {
    // 1. Register PropertyBag (data store)
    auto* bag = componentSystem.registerComponent<PropertyBag>("bag");
    if (!bag) {
        CCLOG("[PropertyEditor] Failed to register PropertyBag component");
        return false;
    }

    // 2. Register PropertyEditorView (ImGui UI), inject bag via config
    std::unordered_map<std::string, std::any> viewCfg;
    viewCfg["bag"] = bag;
    auto* view = componentSystem.registerComponent<PropertyEditorView>("view", viewCfg);
    if (!view) {
        CCLOG("[PropertyEditor] Failed to register PropertyEditorView component");
        return false;
    }

    // 3. Register PropertyEventHandler (event listener), inject bag via config
    std::unordered_map<std::string, std::any> handlerCfg;
    handlerCfg["bag"] = bag;
    auto* handler = componentSystem.registerComponent<PropertyEventHandler>("events", handlerCfg);
    if (!handler) {
        CCLOG("[PropertyEditor] Failed to register PropertyEventHandler component");
        return false;
    }

    CCLOG("[PropertyEditor] Initialized with 3 components: bag, view, events");
    return true;
}

void PropertyEditorPlugin::update(float deltaTime) {
    // Run API tests once (they only accumulate, not re-run)
    static bool apiTestsRun = false;
    if (!apiTestsRun) {
        TestFramework::getInstance().runApiTests("PropertyEditor");
        apiTestsRun = true;
    }

    componentSystem.updateAll(deltaTime);
}

void PropertyEditorPlugin::shutdown() {
    IPlugin::shutdown();
    CCLOG("[PropertyEditor] Shutdown");
}
