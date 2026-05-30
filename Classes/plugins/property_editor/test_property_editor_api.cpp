// test_property_editor_api.cpp - API tests for PropertyEditor plugin
//
// Tests PropertyBag set/get/has/existence/terminate operations.
// PropertyBag::initialize() sets default properties: Name="Hero", X=100, Y=200, Rotation=0.0

#include "PropertyEditorPlugin.h"
#include "tests/TestFramework.h"

// ================================================================
// API Tests
// ================================================================

static bool test_property_bag_has_defaults() {
    PropertyBag bag;
    bag.initialize();

    // Default properties set by initialize()
    if (!bag.has("Name")) return false;
    if (!bag.has("X")) return false;
    if (!bag.has("Y")) return false;
    if (!bag.has("Rotation")) return false;

    if (bag.getString("Name") != "Hero") return false;
    if (bag.getInt("X") != 100) return false;
    if (bag.getInt("Y") != 200) return false;
    if (bag.getFloat("Rotation") != 0.0f) return false;

    // Should have exactly 4 keys
    auto keys = bag.getAllKeys();
    if (keys.size() != 4) return false;

    return true;
}

static bool test_property_bag_set_get() {
    PropertyBag bag;
    bag.initialize();

    // Overwrite existing defaults
    bag.setInt("X", 42);
    if (bag.getInt("X") != 42) return false;
    if (bag.getAsString("X") != "42") return false;

    bag.setFloat("Rotation", 90.0f);
    if (bag.getFloat("Rotation") != 90.0f) return false;

    bag.setString("Name", "Villain");
    if (bag.getString("Name") != "Villain") return false;

    // Add new properties
    bag.setInt("Health", 999);
    bag.setFloat("Scale", 1.5f);
    bag.setString("Tag", "enemy");

    if (bag.getInt("Health") != 999) return false;
    if (bag.getFloat("Scale") != 1.5f) return false;
    if (bag.getString("Tag") != "enemy") return false;

    // Test getType
    if (bag.getType("Health") != PropertyBag::ValueType::Int) return false;
    if (bag.getType("Scale") != PropertyBag::ValueType::Float) return false;
    if (bag.getType("Tag") != PropertyBag::ValueType::String) return false;

    return true;
}

static bool test_property_bag_missing_defaults() {
    PropertyBag bag;
    bag.initialize();

    // Querying non-existent keys returns default values
    if (bag.has("NonExistent")) return false;
    if (bag.getType("NonExistent") != PropertyBag::ValueType::Unknown) return false;

    if (bag.getInt("Missing", 42) != 42) return false;
    if (bag.getFloat("Missing", 3.14f) != 3.14f) return false;
    if (bag.getString("Missing", "fallback") != "fallback") return false;
    if (bag.getAsString("Missing") != "") return false;

    return true;
}

static bool test_property_bag_terminate() {
    PropertyBag bag;
    bag.initialize();
    bag.setInt("A", 1);
    bag.setString("B", "two");

    if (!bag.has("A")) return false;
    if (!bag.has("B")) return false;

    bag.terminate();

    // After terminate, all properties should be cleared
    if (bag.has("A")) return false;
    if (bag.has("B")) return false;
    if (bag.has("Name")) return false; // defaults also gone
    if (bag.getAllKeys().size() != 0) return false;

    return true;
}

static bool test_property_bag_overwrite_type_change() {
    PropertyBag bag;
    bag.initialize();

    // Change X from int to float
    bag.setFloat("X", 3.14f);
    if (bag.getType("X") != PropertyBag::ValueType::Float) return false;
    if (bag.getFloat("X") != 3.14f) return false;
    // Getting as wrong type returns default
    if (bag.getInt("X", -1) != -1) return false;

    // Change Name from string to int
    bag.setInt("Name", 777);
    if (bag.getType("Name") != PropertyBag::ValueType::Int) return false;
    if (bag.getInt("Name") != 777) return false;
    if (bag.getString("Name", "fallback") != "fallback") return false;

    return true;
}

REGISTER_API_TEST("PropertyEditor", test_property_bag_has_defaults);
REGISTER_API_TEST("PropertyEditor", test_property_bag_set_get);
REGISTER_API_TEST("PropertyEditor", test_property_bag_missing_defaults);
REGISTER_API_TEST("PropertyEditor", test_property_bag_terminate);
REGISTER_API_TEST("PropertyEditor", test_property_bag_overwrite_type_change);
