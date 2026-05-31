// PropertyEditorPlugin.cpp - Property Editor Plugin implementation
//
// Components:
//   1. PropertyBag         — typed property store (int/float/string/bool/Vec2/Color3B/Enum)
//   2. UndoStack           — undo/redo for property changes
//   3. PropertyEditorView  — ImGui window with search filter, rich editors, undo/redo
//   4. PropertyEventHandler — listens to NodeSelectedEvent
//
// Plugin initialize() wires them together through ComponentSystem.

#include "PropertyEditorPlugin.h"
#include "editor/EditorEvents.h"
#include "tests/TestFramework.h"
#include "cocos2d.h"
#include "imgui.h"
#include <cstring>
#include <sstream>
#include <algorithm>
#include <cctype>

// ================================================================
// PropertyBag
// ================================================================

bool PropertyBag::initialize(const std::unordered_map<std::string, std::any>&) {
    setString("Name", "Hero");
    setInt("X", 100);
    setInt("Y", 200);
    setFloat("Rotation", 0.0f);
    setBool("Visible", true);
    setVec2("Scale", cocos2d::Vec2(1.0f, 1.0f));
    setColor3B("Color", cocos2d::Color3B(255, 128, 64));
    setEnum("State", 0, {"Idle", "Run", "Attack", "Death"});
    return true;
}

void PropertyBag::update(float) {}

void PropertyBag::terminate() {
    properties_.clear();
}

// --- Typed setters ---

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

void PropertyBag::setBool(const std::string& key, bool value) {
    auto& pv = properties_[key];
    pv.type = StoredType::Bool;
    pv.boolVal = value;
}

void PropertyBag::setVec2(const std::string& key, const cocos2d::Vec2& value) {
    auto& pv = properties_[key];
    pv.type = StoredType::Vec2;
    pv.vec2_x = value.x;
    pv.vec2_y = value.y;
}

void PropertyBag::setColor3B(const std::string& key, const cocos2d::Color3B& value) {
    auto& pv = properties_[key];
    pv.type = StoredType::Color3B;
    pv.color_r = value.r;
    pv.color_g = value.g;
    pv.color_b = value.b;
}

void PropertyBag::setEnum(const std::string& key, int index,
                          const std::vector<std::string>& options) {
    auto& pv = properties_[key];
    pv.type = StoredType::Enum;
    pv.enumIndex = index;
    pv.enumOptions = options;
}

// --- Typed getters ---

int PropertyBag::getInt(const std::string& key, int defaultVal) const {
    auto it = properties_.find(key);
    if (it != properties_.end() && it->second.type == StoredType::Int)
        return it->second.intVal;
    return defaultVal;
}

float PropertyBag::getFloat(const std::string& key, float defaultVal) const {
    auto it = properties_.find(key);
    if (it != properties_.end() && it->second.type == StoredType::Float)
        return it->second.floatVal;
    return defaultVal;
}

std::string PropertyBag::getString(const std::string& key,
                                   const std::string& defaultVal) const {
    auto it = properties_.find(key);
    if (it != properties_.end() && it->second.type == StoredType::String)
        return it->second.stringVal;
    return defaultVal;
}

bool PropertyBag::getBool(const std::string& key, bool defaultVal) const {
    auto it = properties_.find(key);
    if (it != properties_.end() && it->second.type == StoredType::Bool)
        return it->second.boolVal;
    return defaultVal;
}

cocos2d::Vec2 PropertyBag::getVec2(const std::string& key,
                                    const cocos2d::Vec2& defaultVal) const {
    auto it = properties_.find(key);
    if (it != properties_.end() && it->second.type == StoredType::Vec2)
        return cocos2d::Vec2(it->second.vec2_x, it->second.vec2_y);
    return defaultVal;
}

cocos2d::Color3B PropertyBag::getColor3B(const std::string& key,
                                          const cocos2d::Color3B& defaultVal) const {
    auto it = properties_.find(key);
    if (it != properties_.end() && it->second.type == StoredType::Color3B)
        return cocos2d::Color3B(it->second.color_r, it->second.color_g, it->second.color_b);
    return defaultVal;
}

int PropertyBag::getEnumIndex(const std::string& key, int defaultVal) const {
    auto it = properties_.find(key);
    if (it != properties_.end() && it->second.type == StoredType::Enum)
        return it->second.enumIndex;
    return defaultVal;
}

const std::vector<std::string>* PropertyBag::getEnumOptions(const std::string& key) const {
    auto it = properties_.find(key);
    if (it != properties_.end() && it->second.type == StoredType::Enum)
        return &it->second.enumOptions;
    return nullptr;
}

bool PropertyBag::has(const std::string& key) const {
    return properties_.find(key) != properties_.end();
}

std::vector<std::string> PropertyBag::getAllKeys() const {
    std::vector<std::string> keys;
    for (const auto& [key, _] : properties_)
        keys.push_back(key);
    return keys;
}

std::string PropertyBag::getAsString(const std::string& key) const {
    auto it = properties_.find(key);
    if (it == properties_.end()) return "";
    switch (it->second.type) {
        case StoredType::Int:
            return std::to_string(it->second.intVal);
        case StoredType::Float: {
            char buf[32];
            snprintf(buf, sizeof(buf), "%.2f", it->second.floatVal);
            return buf;
        }
        case StoredType::String:
            return it->second.stringVal;
        case StoredType::Bool:
            return it->second.boolVal ? "true" : "false";
        case StoredType::Vec2: {
            char buf[64];
            snprintf(buf, sizeof(buf), "%.2f, %.2f", it->second.vec2_x, it->second.vec2_y);
            return buf;
        }
        case StoredType::Color3B: {
            char buf[32];
            snprintf(buf, sizeof(buf), "%d, %d, %d",
                     it->second.color_r, it->second.color_g, it->second.color_b);
            return buf;
        }
        case StoredType::Enum: {
            if (it->second.enumIndex >= 0 &&
                it->second.enumIndex < (int)it->second.enumOptions.size())
                return it->second.enumOptions[it->second.enumIndex];
            return std::to_string(it->second.enumIndex);
        }
    }
    return "";
}

void PropertyBag::setFromString(const std::string& key, const std::string& value) {
    auto it = properties_.find(key);
    if (it == properties_.end()) return;
    switch (it->second.type) {
        case StoredType::Int:
            setInt(key, std::stoi(value));
            break;
        case StoredType::Float:
            setFloat(key, std::stof(value));
            break;
        case StoredType::String:
            setString(key, value);
            break;
        case StoredType::Bool:
            setBool(key, value == "true" || value == "1");
            break;
        case StoredType::Vec2: {
            float x = 0.0f, y = 0.0f;
            sscanf(value.c_str(), "%f, %f", &x, &y);
            setVec2(key, cocos2d::Vec2(x, y));
            break;
        }
        case StoredType::Color3B: {
            int r = 255, g = 255, b = 255;
            sscanf(value.c_str(), "%d, %d, %d", &r, &g, &b);
            setColor3B(key, cocos2d::Color3B(
                static_cast<GLubyte>(r),
                static_cast<GLubyte>(g),
                static_cast<GLubyte>(b)));
            break;
        }
        case StoredType::Enum: {
            int idx = std::stoi(value);
            const auto& options = it->second.enumOptions;
            if (idx >= 0 && idx < static_cast<int>(options.size()))
                setEnum(key, idx, options);
            break;
        }
    }
}

PropertyBag::ValueType PropertyBag::getType(const std::string& key) const {
    auto it = properties_.find(key);
    if (it == properties_.end()) return ValueType::Unknown;
    switch (it->second.type) {
        case StoredType::Int:     return ValueType::Int;
        case StoredType::Float:   return ValueType::Float;
        case StoredType::String:  return ValueType::String;
        case StoredType::Bool:    return ValueType::Bool;
        case StoredType::Vec2:    return ValueType::Vec2;
        case StoredType::Color3B: return ValueType::Color3B;
        case StoredType::Enum:    return ValueType::Enum;
    }
    return ValueType::Unknown;
}

// ================================================================
// UndoStack
// ================================================================

void UndoStack::push(const std::string& key, const std::string& oldVal,
                     const std::string& newVal) {
    // Truncate any redo entries ahead of current position
    while (index_ + 1 < (int)entries_.size())
        entries_.pop_back();
    entries_.push_back({key, oldVal, newVal});
    index_ = static_cast<int>(entries_.size()) - 1;
}

bool UndoStack::canUndo() const {
    return index_ >= 0;
}

bool UndoStack::canRedo() const {
    return index_ + 1 < (int)entries_.size();
}

UndoStack::UndoEntry UndoStack::undo() {
    auto entry = entries_[index_];
    index_--;
    return entry;
}

UndoStack::UndoEntry UndoStack::redo() {
    index_++;
    return entries_[index_];
}

void UndoStack::clear() {
    entries_.clear();
    index_ = -1;
}

size_t UndoStack::stackSize() const {
    return entries_.size();
}

size_t UndoStack::currentIndex() const {
    return static_cast<size_t>(index_ >= 0 ? index_ : 0);
}

// ================================================================
// PropertyEditorView
// ================================================================

bool PropertyEditorView::initialize(const std::unordered_map<std::string, std::any>& config) {
    auto itBag = config.find("bag");
    if (itBag != config.end())
        bag_ = std::any_cast<PropertyBag*>(itBag->second);

    auto itUndo = config.find("undoStack");
    if (itUndo != config.end())
        undoStack_ = std::any_cast<UndoStack*>(itUndo->second);

    std::memset(filterBuf_, 0, sizeof(filterBuf_));
    return bag_ != nullptr && undoStack_ != nullptr;
}

void PropertyEditorView::update(float) {
    if (!showWindow_ || !bag_ || !undoStack_) return;

    // Ctrl+Z / Ctrl+Y hotkeys
    auto& io = ImGui::GetIO();
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z, false))
        applyUndo();
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Y, false))
        applyRedo();

    ImGui::Begin("Property Editor", &showWindow_, ImGuiWindowFlags_NoDocking);

    renderToolbar();
    ImGui::Separator();
    renderPropertyTable();

    ImGui::End();

    // Run registered GUI tests
    TestFramework::getInstance().runGuiTests("PropertyEditor");
}

void PropertyEditorView::terminate() {
    bag_ = nullptr;
    undoStack_ = nullptr;
}

void PropertyEditorView::renderToolbar() {
    // Undo / Redo buttons
    ImGui::BeginDisabled(!undoStack_->canUndo());
    if (ImGui::Button("Undo") || (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_Z)))
        applyUndo();
    ImGui::EndDisabled();

    ImGui::SameLine();

    ImGui::BeginDisabled(!undoStack_->canRedo());
    if (ImGui::Button("Redo") || (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_Y)))
        applyRedo();
    ImGui::EndDisabled();

    ImGui::SameLine();
    ImGui::TextDisabled("(Ctrl+Z/Y)");

    // Stack info
    ImGui::SameLine();
    ImGui::Text(" | Stack: %zu/%zu",
                undoStack_->currentIndex(), undoStack_->stackSize());
}

void PropertyEditorView::renderPropertyTable() {
    if (!bag_) return;

    // Search filter
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::InputTextWithHint("##filter", "Filter properties...",
                             filterBuf_, sizeof(filterBuf_));
    std::string filter(filterBuf_);

    // Build filtered key list (case-insensitive)
    auto allKeys = bag_->getAllKeys();
    std::vector<std::string> filteredKeys;
    if (filter.empty()) {
        filteredKeys = allKeys;
    } else {
        std::string filterLower = filter;
        std::transform(filterLower.begin(), filterLower.end(),
                       filterLower.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        for (const auto& key : allKeys) {
            std::string keyLower = key;
            std::transform(keyLower.begin(), keyLower.end(),
                           keyLower.begin(),
                           [](unsigned char c) { return std::tolower(c); });
            if (keyLower.find(filterLower) != std::string::npos)
                filteredKeys.push_back(key);
        }
    }

    if (!ImGui::BeginTable("##propTable", 2,
                           ImGuiTableFlags_Borders |
                           ImGuiTableFlags_Resizable |
                           ImGuiTableFlags_RowBg)) {
        return;
    }

    ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 100.0f);
    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableHeadersRow();

    for (const auto& key : filteredKeys) {
        ImGui::PushID(key.c_str());

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextUnformatted(key.c_str());

        ImGui::TableSetColumnIndex(1);
        ImGui::SetNextItemWidth(-FLT_MIN);

        auto type = bag_->getType(key);
        switch (type) {
            case PropertyBag::ValueType::Int:     renderIntEditor(key);     break;
            case PropertyBag::ValueType::Float:   renderFloatEditor(key);   break;
            case PropertyBag::ValueType::String:  renderStringEditor(key);  break;
            case PropertyBag::ValueType::Bool:    renderBoolEditor(key);    break;
            case PropertyBag::ValueType::Vec2:    renderVec2Editor(key);    break;
            case PropertyBag::ValueType::Color3B: renderColor3BEditor(key); break;
            case PropertyBag::ValueType::Enum:    renderEnumEditor(key);    break;
            default:
                ImGui::TextUnformatted("?");
                break;
        }

        ImGui::PopID();
    }

    ImGui::EndTable();
}

void PropertyEditorView::renderIntEditor(const std::string& key) {
    int val = bag_->getInt(key);
    if (ImGui::DragInt("##val", &val, 1.0f, -99999, 99999)) {
        std::string oldVal = bag_->getAsString(key);
        bag_->setInt(key, val);
        publishChange(key, oldVal, std::to_string(val));
    }
}

void PropertyEditorView::renderFloatEditor(const std::string& key) {
    float val = bag_->getFloat(key);
    if (ImGui::DragFloat("##val", &val, 0.01f, 0.0f, 0.0f, "%.2f")) {
        std::string oldVal = bag_->getAsString(key);
        bag_->setFloat(key, val);
        char buf[32];
        snprintf(buf, sizeof(buf), "%.2f", val);
        publishChange(key, oldVal, buf);
    }
}

void PropertyEditorView::renderStringEditor(const std::string& key) {
    char buf[256];
    std::strncpy(buf, bag_->getString(key).c_str(), sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    if (ImGui::InputText("##val", buf, sizeof(buf))) {
        std::string oldVal = bag_->getAsString(key);
        bag_->setString(key, buf);
        publishChange(key, oldVal, buf);
    }
}

void PropertyEditorView::renderBoolEditor(const std::string& key) {
    bool val = bag_->getBool(key);
    if (ImGui::Checkbox("##val", &val)) {
        std::string oldVal = bag_->getAsString(key);
        bag_->setBool(key, val);
        publishChange(key, oldVal, val ? "true" : "false");
    }
}

void PropertyEditorView::renderVec2Editor(const std::string& key) {
    auto val = bag_->getVec2(key);
    float v[2] = { val.x, val.y };
    ImGui::SetNextItemWidth(ImGui::CalcItemWidth() * 0.45f);
    bool changed = ImGui::DragFloat("##x", &v[0], 0.01f, 0.0f, 0.0f, "X: %.2f");
    ImGui::SameLine(0, 4);
    ImGui::SetNextItemWidth(ImGui::CalcItemWidth() * 0.45f);
    changed |= ImGui::DragFloat("##y", &v[1], 0.01f, 0.0f, 0.0f, "Y: %.2f");
    if (changed) {
        std::string oldVal = bag_->getAsString(key);
        bag_->setVec2(key, cocos2d::Vec2(v[0], v[1]));
        char buf[64];
        snprintf(buf, sizeof(buf), "%.2f, %.2f", v[0], v[1]);
        publishChange(key, oldVal, buf);
    }
}

void PropertyEditorView::renderColor3BEditor(const std::string& key) {
    auto val = bag_->getColor3B(key);
    float col[3] = { val.r / 255.0f, val.g / 255.0f, val.b / 255.0f };
    if (ImGui::ColorEdit3("##val", col, ImGuiColorEditFlags_NoInputs)) {
        std::string oldVal = bag_->getAsString(key);
        auto newCol = cocos2d::Color3B(
            static_cast<GLubyte>(col[0] * 255.0f),
            static_cast<GLubyte>(col[1] * 255.0f),
            static_cast<GLubyte>(col[2] * 255.0f));
        bag_->setColor3B(key, newCol);
        char buf[32];
        snprintf(buf, sizeof(buf), "%d, %d, %d", newCol.r, newCol.g, newCol.b);
        publishChange(key, oldVal, buf);
    }
}

void PropertyEditorView::renderEnumEditor(const std::string& key) {
    int idx = bag_->getEnumIndex(key);
    auto* options = bag_->getEnumOptions(key);
    if (!options) {
        ImGui::TextUnformatted("?");
        return;
    }

    const char* preview = (idx >= 0 && idx < (int)options->size())
                          ? (*options)[idx].c_str() : "?";

    if (ImGui::BeginCombo("##val", preview)) {
        for (int i = 0; i < (int)options->size(); i++) {
            bool isSelected = (idx == i);
            if (ImGui::Selectable((*options)[i].c_str(), isSelected)) {
                std::string oldVal = bag_->getAsString(key);
                bag_->setEnum(key, i, *options);
                publishChange(key, oldVal, std::to_string(i));
            }
            if (isSelected)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
}

void PropertyEditorView::publishChange(const std::string& key,
                                        const std::string& oldVal,
                                        const std::string& newVal) {
    CCLOG("[PropertyEditor] '%s': %s -> %s", key.c_str(), oldVal.c_str(), newVal.c_str());

    // Push to undo stack
    undoStack_->push(key, oldVal, newVal);

    // Publish event
    PropertyChangedEvent evt;
    evt.key = key;
    evt.oldValue = oldVal;
    evt.newValue = newVal;
    ugf::EventBus::getInstance().publish(evt);
}

void PropertyEditorView::applyUndo() {
    if (!undoStack_->canUndo()) return;
    auto entry = undoStack_->undo();
    bag_->setFromString(entry.key, entry.oldValue);
    CCLOG("[PropertyEditor] Undo '%s': restored to '%s'",
          entry.key.c_str(), entry.oldValue.c_str());

    PropertyChangedEvent evt;
    evt.key = entry.key;
    evt.oldValue = entry.newValue;
    evt.newValue = entry.oldValue;
    ugf::EventBus::getInstance().publish(evt);
}

void PropertyEditorView::applyRedo() {
    if (!undoStack_->canRedo()) return;
    auto entry = undoStack_->redo();
    bag_->setFromString(entry.key, entry.newValue);
    CCLOG("[PropertyEditor] Redo '%s': restored to '%s'",
          entry.key.c_str(), entry.newValue.c_str());

    PropertyChangedEvent evt;
    evt.key = entry.key;
    evt.oldValue = entry.oldValue;
    evt.newValue = entry.newValue;
    ugf::EventBus::getInstance().publish(evt);
}

// ================================================================
// PropertyEventHandler
// ================================================================

bool PropertyEventHandler::initialize(const std::unordered_map<std::string, std::any>& config) {
    auto it = config.find("bag");
    if (it != config.end())
        bag_ = std::any_cast<PropertyBag*>(it->second);

    nodeSelectedConn_ = ugf::EventBus::getInstance().subscribe<NodeSelectedEvent>(
        [this](const NodeSelectedEvent& event) {
            onNodeSelected(event);
        });

    return true;
}

void PropertyEventHandler::update(float) {}

void PropertyEventHandler::terminate() {
    nodeSelectedConn_.release();
    bag_ = nullptr;
}

void PropertyEventHandler::onNodeSelected(const NodeSelectedEvent& event) {
    CCLOG("[PropertyEditor] NodeSelected: %s", event.nodeName.c_str());
    if (bag_)
        bag_->setString("Name", event.nodeName);
}

// ================================================================
// PropertyEditorPlugin
// ================================================================

bool PropertyEditorPlugin::initialize() {
    // 1. Register PropertyBag (data store)
    auto* bag = componentSystem.registerComponent<PropertyBag>("bag");
    if (!bag) {
        CCLOG("[PropertyEditor] Failed to register PropertyBag component");
        return false;
    }

    // 2. Create UndoStack (owned by plugin, pointer passed to view)
    undoStack_ = std::make_unique<UndoStack>();

    // 3. Register PropertyEditorView, inject bag + undoStack via config
    std::unordered_map<std::string, std::any> viewCfg;
    viewCfg["bag"] = bag;
    viewCfg["undoStack"] = undoStack_.get();
    auto* view = componentSystem.registerComponent<PropertyEditorView>("view", viewCfg);
    if (!view) {
        CCLOG("[PropertyEditor] Failed to register PropertyEditorView component");
        return false;
    }

    // 4. Register PropertyEventHandler, inject bag via config
    std::unordered_map<std::string, std::any> handlerCfg;
    handlerCfg["bag"] = bag;
    auto* handler = componentSystem.registerComponent<PropertyEventHandler>("events", handlerCfg);
    if (!handler) {
        CCLOG("[PropertyEditor] Failed to register PropertyEventHandler component");
        return false;
    }

    CCLOG("[PropertyEditor] Initialized with 3 components: bag, view, events + undo stack");
    return true;
}

void PropertyEditorPlugin::update(float deltaTime) {
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
