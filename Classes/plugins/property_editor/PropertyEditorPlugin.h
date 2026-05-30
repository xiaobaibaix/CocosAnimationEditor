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
#include <string>
#include <unordered_map>
#include <any>
#include <vector>
#include <memory>

// ================================================================
// PropertyBag — data component: string → variant (int|float|string)
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

    // Typed getters (with defaults)
    int getInt(const std::string& key, int defaultVal = 0) const;
    float getFloat(const std::string& key, float defaultVal = 0.0f) const;
    std::string getString(const std::string& key, const std::string& defaultVal = "") const;

    // Check existence
    bool has(const std::string& key) const;

    // Get all keys
    std::vector<std::string> getAllKeys() const;

    // Get value as string (for display)
    std::string getAsString(const std::string& key) const;

    // Type enum for UI rendering
    enum class ValueType { Int, Float, String, Unknown };
    ValueType getType(const std::string& key) const;

private:
    enum class StoredType { Int, Float, String };

    struct PropertyValue {
        StoredType type = StoredType::Int;
        int intVal = 0;
        float floatVal = 0.0f;
        std::string stringVal;
    };

    std::unordered_map<std::string, PropertyValue> properties_;
};

// ================================================================
// PropertyEditorView — ImGui UI component
//   Renders "Property Editor" window with a Property | Value table.
//   Gets PropertyBag* from init config. Publishes PropertyChangedEvent on edit.
// ================================================================

class PropertyEditorView : public ugf::Component {
public:
    PropertyEditorView() = default;

    std::string getComponentId() const override { return "view"; }

    bool initialize(const std::unordered_map<std::string, std::any>& config = {}) override;
    void update(float deltaTime) override;
    void terminate() override;

private:
    void renderPropertyTable();

    PropertyBag* bag_ = nullptr;
    bool showWindow_ = true;
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
};

#endif // PROPERTY_EDITOR_PLUGIN_H
