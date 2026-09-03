#pragma once

#include <cstdint>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "pl/Export.hpp"

namespace pl::modmenu {

enum class ConfigControlTypeV2 {
    Toggle,
    SliderInt,
    SliderFloat,
    RangeSlider,
    Choice,
    MultiChoice,
    ToggleGroup,
    OrderedList,
    Color,
    Keybind,
    Text,
    MultilineText,
    Button,
    Info,
    Section,
};

enum class ConfigChoiceStyleV2 {
    Auto,
    Radio,
    Segmented,
    Dropdown,
    Chips,
    Checklist,
};

enum class ConfigConditionOpV2 {
    Equals,
    NotEquals,
    Truthy,
    Falsy,
    Contains,
};

struct ConfigCategoryV2 {
    std::string id;
    std::string title;
    std::string description;
};

struct ConfigOptionV2 {
    std::string value;
    std::string label;
    std::string description;
    std::string key;
    bool disabled{};
    std::string currentValue;
    bool hasCurrentValue{};
};

struct ConfigConditionV2 {
    std::string key;
    ConfigConditionOpV2 op{ConfigConditionOpV2::Equals};
    std::string value;
};

struct ConfigNodeV2 {
    std::string id;
    std::string key;
    std::string category;
    std::string title;
    std::string description;
    std::string section;
    ConfigControlTypeV2 type{ConfigControlTypeV2::Toggle};
    ConfigChoiceStyleV2 choiceStyle{ConfigChoiceStyleV2::Auto};
    std::string defaultValue;
    std::string currentValue;
    bool hasCurrentValue{};
    std::string minValue;
    std::string maxValue;
    std::string step;
    std::string unit;
    std::string placeholder;
    std::string actionValue{"true"};
    int maxLength{};
    bool advanced{};
    bool disabled{};
    bool searchable{};
    bool allowReorder{};
    bool colorAlpha{true};
    bool collapsible{};
    std::vector<ConfigOptionV2> options;
    std::vector<ConfigConditionV2> visibleWhen;
    std::vector<ConfigConditionV2> enabledWhen;
};

class ConfigSchemaBuilder {
public:
    ConfigSchemaBuilder& category(std::string id, std::string title, std::string description = {}) {
        mCategories.push_back({std::move(id), std::move(title), std::move(description)});
        return *this;
    }

    ConfigSchemaBuilder& node(ConfigNodeV2 value) {
        mNodes.push_back(std::move(value));
        return *this;
    }

    ConfigSchemaBuilder& defaultCategory(std::string id) {
        mDefaultCategory = std::move(id);
        return *this;
    }

    [[nodiscard]] std::string toJson() const {
        std::ostringstream out;
        out << "{\"version\":2,\"default_category\":";
        writeString(out, mDefaultCategory);
        out << ",\"categories\":[";
        for (std::size_t i = 0; i < mCategories.size(); ++i) {
            if (i) out << ',';
            const auto& category = mCategories[i];
            out << "{\"id\":";
            writeString(out, category.id);
            out << ",\"title\":";
            writeString(out, category.title);
            out << ",\"description\":";
            writeString(out, category.description);
            out << '}';
        }
        out << "],\"nodes\":[";
        for (std::size_t i = 0; i < mNodes.size(); ++i) {
            if (i) out << ',';
            writeNode(out, mNodes[i]);
        }
        out << "]}";
        return out.str();
    }

private:
    static std::string_view typeName(ConfigControlTypeV2 type) {
        switch (type) {
            case ConfigControlTypeV2::Toggle: return "toggle";
            case ConfigControlTypeV2::SliderInt: return "slider_int";
            case ConfigControlTypeV2::SliderFloat: return "slider_float";
            case ConfigControlTypeV2::RangeSlider: return "range_slider";
            case ConfigControlTypeV2::Choice: return "choice";
            case ConfigControlTypeV2::MultiChoice: return "multi_choice";
            case ConfigControlTypeV2::ToggleGroup: return "toggle_group";
            case ConfigControlTypeV2::OrderedList: return "ordered_list";
            case ConfigControlTypeV2::Color: return "color";
            case ConfigControlTypeV2::Keybind: return "keybind";
            case ConfigControlTypeV2::Text: return "text";
            case ConfigControlTypeV2::MultilineText: return "multiline_text";
            case ConfigControlTypeV2::Button: return "button";
            case ConfigControlTypeV2::Info: return "info";
            case ConfigControlTypeV2::Section: return "section";
        }
        return "info";
    }

    static std::string_view styleName(ConfigChoiceStyleV2 style) {
        switch (style) {
            case ConfigChoiceStyleV2::Auto: return "auto";
            case ConfigChoiceStyleV2::Radio: return "radio";
            case ConfigChoiceStyleV2::Segmented: return "segmented";
            case ConfigChoiceStyleV2::Dropdown: return "dropdown";
            case ConfigChoiceStyleV2::Chips: return "chips";
            case ConfigChoiceStyleV2::Checklist: return "checklist";
        }
        return "auto";
    }

    static std::string_view opName(ConfigConditionOpV2 op) {
        switch (op) {
            case ConfigConditionOpV2::Equals: return "equals";
            case ConfigConditionOpV2::NotEquals: return "not_equals";
            case ConfigConditionOpV2::Truthy: return "truthy";
            case ConfigConditionOpV2::Falsy: return "falsy";
            case ConfigConditionOpV2::Contains: return "contains";
        }
        return "equals";
    }

    static void writeString(std::ostringstream& out, std::string_view value) {
        out << '"';
        for (unsigned char c : value) {
            switch (c) {
                case '"': out << "\\\""; break;
                case '\\': out << "\\\\"; break;
                case '\b': out << "\\b"; break;
                case '\f': out << "\\f"; break;
                case '\n': out << "\\n"; break;
                case '\r': out << "\\r"; break;
                case '\t': out << "\\t"; break;
                default:
                    if (c < 0x20) {
                        static constexpr char hex[] = "0123456789ABCDEF";
                        out << "\\u00" << hex[(c >> 4) & 0xF] << hex[c & 0xF];
                    } else {
                        out << static_cast<char>(c);
                    }
                    break;
            }
        }
        out << '"';
    }

    static void writeConditions(std::ostringstream& out, const std::vector<ConfigConditionV2>& conditions) {
        out << '[';
        for (std::size_t i = 0; i < conditions.size(); ++i) {
            if (i) out << ',';
            out << "{\"key\":";
            writeString(out, conditions[i].key);
            out << ",\"op\":";
            writeString(out, opName(conditions[i].op));
            out << ",\"value\":";
            writeString(out, conditions[i].value);
            out << '}';
        }
        out << ']';
    }

    static void writeNode(std::ostringstream& out, const ConfigNodeV2& node) {
        out << "{\"id\":";
        writeString(out, node.id);
        out << ",\"key\":";
        writeString(out, node.key);
        out << ",\"category\":";
        writeString(out, node.category);
        out << ",\"title\":";
        writeString(out, node.title);
        out << ",\"description\":";
        writeString(out, node.description);
        out << ",\"section\":";
        writeString(out, node.section);
        out << ",\"type\":";
        writeString(out, typeName(node.type));
        out << ",\"style\":";
        writeString(out, styleName(node.choiceStyle));
        out << ",\"default_value\":";
        writeString(out, node.defaultValue);
        if (node.hasCurrentValue || !node.currentValue.empty()) {
            out << ",\"current_value\":";
            writeString(out, node.currentValue);
        }
        out << ",\"min_value\":";
        writeString(out, node.minValue);
        out << ",\"max_value\":";
        writeString(out, node.maxValue);
        out << ",\"step\":";
        writeString(out, node.step);
        out << ",\"unit\":";
        writeString(out, node.unit);
        out << ",\"placeholder\":";
        writeString(out, node.placeholder);
        out << ",\"action_value\":";
        writeString(out, node.actionValue);
        out << ",\"max_length\":" << node.maxLength;
        out << ",\"advanced\":" << (node.advanced ? "true" : "false");
        out << ",\"disabled\":" << (node.disabled ? "true" : "false");
        out << ",\"searchable\":" << (node.searchable ? "true" : "false");
        out << ",\"allow_reorder\":" << (node.allowReorder ? "true" : "false");
        out << ",\"color_alpha\":" << (node.colorAlpha ? "true" : "false");
        out << ",\"collapsible\":" << (node.collapsible ? "true" : "false");
        out << ",\"options\":[";
        for (std::size_t i = 0; i < node.options.size(); ++i) {
            if (i) out << ',';
            const auto& option = node.options[i];
            out << "{\"value\":";
            writeString(out, option.value);
            out << ",\"label\":";
            writeString(out, option.label);
            out << ",\"description\":";
            writeString(out, option.description);
            out << ",\"key\":";
            writeString(out, option.key);
            out << ",\"disabled\":" << (option.disabled ? "true" : "false");
            if (option.hasCurrentValue || !option.currentValue.empty()) {
                out << ",\"current_value\":";
                writeString(out, option.currentValue);
            }
            out << '}';
        }
        out << "],\"visible_when\":";
        writeConditions(out, node.visibleWhen);
        out << ",\"enabled_when\":";
        writeConditions(out, node.enabledWhen);
        out << '}';
    }

    std::vector<ConfigCategoryV2> mCategories;
    std::vector<ConfigNodeV2> mNodes;
    std::string mDefaultCategory;
};

PL_EXPORT bool setConfigSchemaJson(std::string_view moduleId, std::string_view schemaJson);
PL_EXPORT void clearConfigSchema(std::string_view moduleId);

}
