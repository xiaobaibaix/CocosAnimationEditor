// PropertyEditorPlugin.h - Property Editor Plugin (UGF multi-component architecture)
//
// Architecture:
//   PropertyEditorPlugin
//     +-- PropertyBag          (data component)
//     +-- PropertyEditorView   (ImGui UI component)
//     +-- PropertyEventHandler (event listener component)
//
// Components communicate through shared PropertyBag pointer passed via config.
// View publishes PropertyChangedEvent; Handler subscribes to NodeSelectedEvent.

#ifndef PROPERTY_EDITOR_PLUGIN_H
#define PROPERTY_EDITOR_PLUGIN_H

#include "PluginSystem.hpp"
#include "ComponentSystem.hpp"
#include "EventSystem.hpp"
#include "math/Vec2.h"
#include "base/ccTypes.h"
#include <string>
#include <unordered_map>
#include <any>
#include <vector>
#include <memory>

// ================================================================
// PropertyBag — data component: string -> variant property store
// ================================================================

class PropertyBag : public ugf::Component {
public:
    PropertyBag() = default;

    std::string getComponentId() const override { return "bag"; }

    bool initialize(const std::unordered_map<std::string, std::any>& config = {}) override;
    void update(float deltaTime) override;
    void terminate() override;

    // Typed setters
    void setInt(const std::string& key, int value);
    void setFloat(const std::string& key, float value);
    void setString(const std::string& key, const std::string& value);
    void setBool(const std::string& key, bool value);
    void setVec2(const std::string& key, const cocos2d::Vec2& value);
    void setColor3B(const std::string& key, const cocos2d::Color3B& value);
    void setEnum(const std::string& key, int index, const std::vector<std::string>& options);

    // Typed getters (with defaults)
    int getInt(const std::string& key, int defaultVal = 0) const;
    float getFloat(const std::string& key, float defaultVal = 0.0f) const;
    std::string getString(const std::string& key, const std::string& defaultVal = "") const;
    bool getBool(const std::string& key, bool defaultVal = false) const;
    cocos2d::Vec2 getVec2(const std::string& key, const cocos2d::Vec2& defaultVal = cocos2d::Vec2::ZERO) const;
    cocos2d::Color3B getColor3B(const std::string& key, const cocos2d::Color3B& defaultVal = cocos2d::Color3B::WHITE) const;
    int getEnumIndex(const std::string& key, int defaultVal = 0) const;
    const std::vector<std::string>* getEnumOptions(const std::string& key) const;

    // Check existence
    bool has(const std::string& key) const;

    // Get all keys
    std::vector<std::string> getAllKeys() const;

    // Get value as string (for display / serialization)
    std::string getAsString(const std::string& key) const;

    // Set value from its string representation (uses type info to parse)
    void setFromString(const std::string& key, const std::string& value);

    // Type enum for UI rendering
    enum class ValueType { Int, Float, String, Bool, Vec2, Color3B, Enum, Unknown };
    ValueType getType(const std::string& key) const;

private:
    enum class StoredType { Int, Float, String, Bool, Vec2, Color3B, Enum };

    struct PropertyValue {
        StoredType type = StoredType::Int;
        int intVal = 0;
        float floatVal = 0.0f;
        std::string stringVal;
        bool boolVal = false;
        float vec2_x = 0.0f, vec2_y = 0.0f;
        GLubyte color_r = 255, color_g = 255, color_b = 255;
        int enumIndex = 0;
        std::vector<std::string> enumOptions;
    };

    std::unordered_map<std::string, PropertyValue> properties_;
};

// ================================================================
// UndoStack — simple Undo/Redo for property changes
// ================================================================

class UndoStack {
public:
    void push(const std::string& key, const std::string& oldVal, const std::string& newVal);
    bool canUndo() const;
    bool canRedo() const;
    // Returns {key, oldValue, newValue} — the value to restore on undo
    struct UndoEntry { std::string key, oldValue, newValue; };
    UndoEntry undo();
    UndoEntry redo();
    void clear();
    size_t stackSize() const;
    size_t currentIndex() const;

private:
    std::vector<UndoEntry> entries_;
    int index_ = -1;
};

// ================================================================
// PropertyEditorView — ImGui UI component
//   Renders "Property Editor" window with a search filter,
//   per-type editing controls, and Undo/Redo toolbar.
//   Gets PropertyBag* and UndoStack* from init config.
// ================================================================

class PropertyEditorView : public ugf::Component {
public:
    PropertyEditorView() = default;

    std::string getComponentId() const override { return "view"; }

    bool initialize(const std::unordered_map<std::string, std::any>& config = {}) override;
    void update(float deltaTime) override;
    void terminate() override;

private:
    void renderToolbar();
    void renderPropertyTable();
    void renderIntEditor(const std::string& key);
    void renderFloatEditor(const std::string& key);
    void renderStringEditor(const std::string& key);
    void renderBoolEditor(const std::string& key);
    void renderVec2Editor(const std::string& key);
    void renderColor3BEditor(const std::string& key);
    void renderEnumEditor(const std::string& key);
    void publishChange(const std::string& key,
                       const std::string& oldVal, const std::string& newVal);
    void applyUndo();
    void applyRedo();

    PropertyBag* bag_ = nullptr;
    UndoStack* undoStack_ = nullptr;
    bool showWindow_ = true;
    char filterBuf_[128] = {};
};

// ================================================================
// PropertyEventHandler — event listener component
//   Subscribes to NodeSelectedEvent and updates PropertyBag accordingly.
//   Gets PropertyBag* from init config.
// ================================================================

class PropertyEventHandler : public ugf::Component {
public:
    PropertyEventHandler() = default;

    std::string getComponentId() const override { return "events"; }

    bool initialize(const std::unordered_map<std::string, std::any>& config = {}) override;
    void update(float deltaTime) override;
    void terminate() override;

private:
    void onNodeSelected(const struct NodeSelectedEvent& event);

    PropertyBag* bag_ = nullptr;
    ugf::EventConnection nodeSelectedConn_;
};

// ================================================================
// PropertyEditorPlugin — wires Bag + View + Events into one plugin
// ================================================================

class PropertyEditorPlugin : public ugf::IPlugin {
public:
    std::string getId() const override { return "PropertyEditor"; }
    bool initialize() override;
    void update(float deltaTime) override;
    void shutdown() override;

private:
    std::unique_ptr<UndoStack> undoStack_;
};

#endif // PROPERTY_EDITOR_PLUGIN_H
