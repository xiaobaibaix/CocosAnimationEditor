// test_property_editor_api.cpp - API and GUI tests for PropertyEditor plugin
//
// API tests: PropertyBag set/get for all types, UndoStack push/undo/redo/clear
// GUI tests: property_types, filter, undo_redo rendering verification

#include "PropertyEditorPlugin.h"
#include "tests/TestFramework.h"
#include "imgui.h"
#include <cstdio>

// ================================================================
// API Tests — PropertyBag
// ================================================================

static bool test_property_bag_has_defaults() {
    PropertyBag bag;
    bag.initialize();

    if (!bag.has("Name")) return false;
    if (!bag.has("X")) return false;
    if (!bag.has("Y")) return false;
    if (!bag.has("Rotation")) return false;

    if (bag.getString("Name") != "Hero") return false;
    if (bag.getInt("X") != 100) return false;
    if (bag.getInt("Y") != 200) return false;
    if (bag.getFloat("Rotation") != 0.0f) return false;

    auto keys = bag.getAllKeys();
    if (keys.size() != 8) return false; // 4 originals + 4 new types
    return true;
}

static bool test_property_bag_set_get() {
    PropertyBag bag;
    bag.initialize();

    bag.setInt("X", 42);
    if (bag.getInt("X") != 42) return false;
    if (bag.getAsString("X") != "42") return false;

    bag.setFloat("Rotation", 90.0f);
    if (bag.getFloat("Rotation") != 90.0f) return false;

    bag.setString("Name", "Villain");
    if (bag.getString("Name") != "Villain") return false;

    bag.setInt("Health", 999);
    bag.setFloat("Scale", 1.5f);
    bag.setString("Tag", "enemy");

    if (bag.getInt("Health") != 999) return false;
    if (bag.getFloat("Scale") != 1.5f) return false;
    if (bag.getString("Tag") != "enemy") return false;

    if (bag.getType("Health") != PropertyBag::ValueType::Int) return false;
    if (bag.getType("Scale") != PropertyBag::ValueType::Float) return false;
    if (bag.getType("Tag") != PropertyBag::ValueType::String) return false;

    return true;
}

static bool test_property_bag_missing_defaults() {
    PropertyBag bag;
    bag.initialize();

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

    if (bag.has("A")) return false;
    if (bag.has("B")) return false;
    if (bag.has("Name")) return false;
    if (bag.getAllKeys().size() != 0) return false;

    return true;
}

static bool test_property_bag_overwrite_type_change() {
    PropertyBag bag;
    bag.initialize();

    bag.setFloat("X", 3.14f);
    if (bag.getType("X") != PropertyBag::ValueType::Float) return false;
    if (bag.getFloat("X") != 3.14f) return false;
    if (bag.getInt("X", -1) != -1) return false;

    bag.setInt("Name", 777);
    if (bag.getType("Name") != PropertyBag::ValueType::Int) return false;
    if (bag.getInt("Name") != 777) return false;
    if (bag.getString("Name", "fallback") != "fallback") return false;

    return true;
}

// --- Bool ---

static bool test_property_bag_bool() {
    PropertyBag bag;
    bag.initialize();

    bag.setBool("Enabled", true);
    if (!bag.has("Enabled")) return false;
    if (!bag.getBool("Enabled")) return false;
    if (bag.getType("Enabled") != PropertyBag::ValueType::Bool) return false;
    if (bag.getAsString("Enabled") != "true") return false;

    bag.setBool("Enabled", false);
    if (bag.getBool("Enabled")) return false;
    if (bag.getAsString("Enabled") != "false") return false;

    // Default for non-existent
    if (bag.getBool("Missing", true) != true) return false;
    if (bag.getBool("Missing", false) != false) return false;

    return true;
}

// --- Vec2 ---

static bool test_property_bag_vec2() {
    PropertyBag bag;
    bag.initialize();

    auto v = cocos2d::Vec2(3.5f, -2.0f);
    bag.setVec2("Position", v);
    if (!bag.has("Position")) return false;
    if (bag.getType("Position") != PropertyBag::ValueType::Vec2) return false;

    auto got = bag.getVec2("Position");
    if (got.x != 3.5f) return false;
    if (got.y != -2.0f) return false;

    // getAsString
    auto str = bag.getAsString("Position");
    if (str.empty()) return false;

    // Default for non-existent
    auto def = bag.getVec2("Missing", cocos2d::Vec2(1.0f, 2.0f));
    if (def.x != 1.0f || def.y != 2.0f) return false;

    return true;
}

// --- Color3B ---

static bool test_property_bag_color3b() {
    PropertyBag bag;
    bag.initialize();

    auto c = cocos2d::Color3B(128, 64, 32);
    bag.setColor3B("Tint", c);
    if (!bag.has("Tint")) return false;
    if (bag.getType("Tint") != PropertyBag::ValueType::Color3B) return false;

    auto got = bag.getColor3B("Tint");
    if (got.r != 128 || got.g != 64 || got.b != 32) return false;

    // getAsString should contain the RGB values
    auto str = bag.getAsString("Tint");
    if (str.find("128") == std::string::npos) return false;

    // Default for non-existent
    auto def = bag.getColor3B("Missing", cocos2d::Color3B(255, 0, 255));
    if (def.r != 255 || def.g != 0 || def.b != 255) return false;

    return true;
}

// --- Enum ---

static bool test_property_bag_enum() {
    PropertyBag bag;
    bag.initialize();

    std::vector<std::string> opts = {"Apple", "Banana", "Cherry"};
    bag.setEnum("Fruit", 1, opts);
    if (!bag.has("Fruit")) return false;
    if (bag.getType("Fruit") != PropertyBag::ValueType::Enum) return false;
    if (bag.getEnumIndex("Fruit") != 1) return false;

    auto* gotOpts = bag.getEnumOptions("Fruit");
    if (!gotOpts) return false;
    if (gotOpts->size() != 3) return false;
    if ((*gotOpts)[0] != "Apple") return false;

    // getAsString returns the selected option name
    if (bag.getAsString("Fruit") != "Banana") return false;

    // Default for non-existent
    if (bag.getEnumIndex("Missing", 5) != 5) return false;
    if (bag.getEnumOptions("Missing") != nullptr) return false;

    return true;
}

// --- setFromString ---

static bool test_property_bag_set_from_string() {
    PropertyBag bag;
    bag.initialize();

    // Set up one of each type
    bag.setInt("TestInt", 0);
    bag.setFloat("TestFloat", 0.0f);
    bag.setString("TestStr", "");
    bag.setBool("TestBool", false);
    bag.setVec2("TestVec2", cocos2d::Vec2::ZERO);
    bag.setColor3B("TestColor", cocos2d::Color3B::WHITE);
    bag.setEnum("TestEnum", 0, {"A", "B", "C"});

    // Round-trip each type via setFromString(getAsString)
    auto roundTrip = [&](const std::string& key, const std::string& expected) -> bool {
        std::string s = bag.getAsString(key);
        bag.setFromString(key, s);
        return bag.getAsString(key) == expected;
    };

    bag.setInt("TestInt", 42);
    if (!roundTrip("TestInt", "42")) return false;

    bag.setFloat("TestFloat", 3.14f);
    if (!roundTrip("TestFloat", "3.14")) return false;

    bag.setString("TestStr", "hello");
    if (!roundTrip("TestStr", "hello")) return false;

    bag.setBool("TestBool", true);
    if (!roundTrip("TestBool", "true")) return false;

    bag.setVec2("TestVec2", cocos2d::Vec2(1.5f, 2.5f));
    if (!roundTrip("TestVec2", "1.50, 2.50")) return false;

    bag.setColor3B("TestColor", cocos2d::Color3B(10, 20, 30));
    if (!roundTrip("TestColor", "10, 20, 30")) return false;

    bag.setEnum("TestEnum", 2, {"A", "B", "C"});
    if (!roundTrip("TestEnum", "2")) return false;

    return true;
}

// ================================================================
// API Tests — UndoStack
// ================================================================

static bool test_undo_stack_push_undo_redo() {
    UndoStack stack;
    if (stack.canUndo()) return false;
    if (stack.canRedo()) return false;
    if (stack.stackSize() != 0) return false;

    stack.push("X", "100", "200");
    if (!stack.canUndo()) return false;
    if (stack.canRedo()) return false;
    if (stack.stackSize() != 1) return false;

    auto entry = stack.undo();
    if (entry.key != "X") return false;
    if (entry.oldValue != "100") return false;
    if (entry.newValue != "200") return false;
    if (stack.canUndo()) return false;
    if (!stack.canRedo()) return false;

    entry = stack.redo();
    if (entry.key != "X") return false;
    if (entry.oldValue != "100") return false;

    return true;
}

static bool test_undo_stack_multiple() {
    UndoStack stack;

    stack.push("A", "0", "1");
    stack.push("B", "1", "2");
    stack.push("C", "2", "3");
    if (stack.stackSize() != 3) return false;

    // Undo all
    auto e = stack.undo();
    if (e.key != "C" || e.oldValue != "2" || e.newValue != "3") return false;
    e = stack.undo();
    if (e.key != "B") return false;
    e = stack.undo();
    if (e.key != "A") return false;

    if (stack.canUndo()) return false;
    if (!stack.canRedo()) return false;

    // Redo all
    e = stack.redo();
    if (e.key != "A" || e.newValue != "1") return false;
    e = stack.redo();
    if (e.key != "B") return false;
    e = stack.redo();
    if (e.key != "C") return false;

    if (!stack.canUndo()) return false;
    if (stack.canRedo()) return false;

    return true;
}

static bool test_undo_stack_branch_truncation() {
    UndoStack stack;

    stack.push("A", "0", "1");
    stack.push("B", "1", "2");
    stack.undo(); // back to A
    stack.undo(); // empty

    // New branch: should truncate old redo entries
    stack.push("C", "9", "10");
    if (stack.stackSize() != 1) return false;
    if (stack.canRedo()) return false;

    auto e = stack.undo();
    if (e.key != "C") return false;

    return true;
}

static bool test_undo_stack_clear() {
    UndoStack stack;

    stack.push("A", "0", "1");
    stack.push("B", "1", "2");
    stack.clear();

    if (stack.stackSize() != 0) return false;
    if (stack.canUndo()) return false;
    if (stack.canRedo()) return false;

    return true;
}

// ================================================================
// GUI Tests — renders interactive verification in TestFramework
// ================================================================

static void test_property_types() {
    ImGui::Text("All 7 property types should appear below:");

    static PropertyBag bag;
    static bool init = false;
    if (!init) {
        bag.setInt("Health", 100);
        bag.setFloat("Speed", 5.50f);
        bag.setString("Label", "Player");
        bag.setBool("Alive", true);
        bag.setVec2("Position", cocos2d::Vec2(10.0f, 20.0f));
        bag.setColor3B("Tint", cocos2d::Color3B(255, 128, 0));
        bag.setEnum("State", 0, {"Idle", "Running", "Jumping"});
        init = true;
    }

    // Simple non-editable display of all property types
    ImGui::Text("Health (int): %d", bag.getInt("Health"));
    ImGui::Text("Speed (float): %.2f", bag.getFloat("Speed"));
    ImGui::Text("Label (string): %s", bag.getString("Label").c_str());
    ImGui::Text("Alive (bool): %s", bag.getBool("Alive") ? "true" : "false");
    auto pos = bag.getVec2("Position");
    ImGui::Text("Position (Vec2): %.1f, %.1f", pos.x, pos.y);
    auto col = bag.getColor3B("Tint");
    ImGui::Text("Tint (Color3B): R=%d G=%d B=%d", col.r, col.g, col.b);
    ImGui::Text("State (Enum): %s [idx=%d]",
                bag.getAsString("State").c_str(), bag.getEnumIndex("State"));

    ImGui::Separator();
    ImGui::Text("Types: Int=%s Float=%s String=%s Bool=%s Vec2=%s Color3B=%s Enum=%s",
                bag.getType("Health") == PropertyBag::ValueType::Int     ? "OK" : "FAIL",
                bag.getType("Speed")  == PropertyBag::ValueType::Float   ? "OK" : "FAIL",
                bag.getType("Label")  == PropertyBag::ValueType::String  ? "OK" : "FAIL",
                bag.getType("Alive")  == PropertyBag::ValueType::Bool    ? "OK" : "FAIL",
                bag.getType("Position")==PropertyBag::ValueType::Vec2    ? "OK" : "FAIL",
                bag.getType("Tint")   == PropertyBag::ValueType::Color3B ? "OK" : "FAIL",
                bag.getType("State")  == PropertyBag::ValueType::Enum    ? "OK" : "FAIL");
}

static void test_filter() {
    ImGui::Text("Filter test — demonstrates case-insensitive key filtering.");

    static PropertyBag bag;
    static bool init = false;
    if (!init) {
        bag.setInt("Health", 100);
        bag.setInt("MaxHealth", 500);
        bag.setFloat("Speed", 5.0f);
        bag.setString("Name", "Archer");
        bag.setBool("Visible", true);
        init = true;
    }

    static char filter[64] = {};
    ImGui::InputTextWithHint("##testfilter", "Type to filter (e.g., 'health')", filter, sizeof(filter));

    std::string filterStr(filter);
    auto keys = bag.getAllKeys();
    int shown = 0;
    for (const auto& key : keys) {
        if (!filterStr.empty()) {
            std::string lowerK = key;
            std::string lowerF = filterStr;
            std::transform(lowerK.begin(), lowerK.end(), lowerK.begin(), ::tolower);
            std::transform(lowerF.begin(), lowerF.end(), lowerF.begin(), ::tolower);
            if (lowerK.find(lowerF) == std::string::npos) continue;
        }
        ImGui::BulletText("%s = %s", key.c_str(), bag.getAsString(key).c_str());
        shown++;
    }
    ImGui::Text("Showing %d of %zu properties", shown, keys.size());
}

static void test_undo_redo() {
    ImGui::Text("Undo/Redo test — shows stack state and interactive buttons.");

    static UndoStack stack;
    static int counter = 0;

    if (ImGui::Button("Push Action")) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%d", counter);
        stack.push("key", buf, std::to_string(counter + 1));
        counter++;
    }

    ImGui::SameLine();

    ImGui::BeginDisabled(!stack.canUndo());
    if (ImGui::Button("Undo")) {
        stack.undo();
    }
    ImGui::EndDisabled();

    ImGui::SameLine();

    ImGui::BeginDisabled(!stack.canRedo());
    if (ImGui::Button("Redo")) {
        stack.redo();
    }
    ImGui::EndDisabled();

    ImGui::SameLine();
    if (ImGui::Button("Clear")) {
        stack.clear();
        counter = 0;
    }

    ImGui::Text("Stack size: %zu  Current index: %zu  CanUndo: %s  CanRedo: %s",
                stack.stackSize(),
                stack.currentIndex(),
                stack.canUndo() ? "Yes" : "No",
                stack.canRedo() ? "Yes" : "No");
}

// ================================================================
// Registrations
// ================================================================

// Existing PropertyBag API tests
REGISTER_API_TEST("PropertyEditor", test_property_bag_has_defaults);
REGISTER_API_TEST("PropertyEditor", test_property_bag_set_get);
REGISTER_API_TEST("PropertyEditor", test_property_bag_missing_defaults);
REGISTER_API_TEST("PropertyEditor", test_property_bag_terminate);
REGISTER_API_TEST("PropertyEditor", test_property_bag_overwrite_type_change);

// New PropertyBag type tests
REGISTER_API_TEST("PropertyEditor", test_property_bag_bool);
REGISTER_API_TEST("PropertyEditor", test_property_bag_vec2);
REGISTER_API_TEST("PropertyEditor", test_property_bag_color3b);
REGISTER_API_TEST("PropertyEditor", test_property_bag_enum);
REGISTER_API_TEST("PropertyEditor", test_property_bag_set_from_string);

// UndoStack tests
REGISTER_API_TEST("PropertyEditor", test_undo_stack_push_undo_redo);
REGISTER_API_TEST("PropertyEditor", test_undo_stack_multiple);
REGISTER_API_TEST("PropertyEditor", test_undo_stack_branch_truncation);
REGISTER_API_TEST("PropertyEditor", test_undo_stack_clear);

// GUI tests
REGISTER_GUI_TEST("PropertyEditor", test_property_types);
REGISTER_GUI_TEST("PropertyEditor", test_filter);
REGISTER_GUI_TEST("PropertyEditor", test_undo_redo);
