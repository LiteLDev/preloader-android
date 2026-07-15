#pragma once

/**
 * @file ModMenu.hpp
 * @brief Mod Menu registration API.
 */

#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "pl/Export.hpp"

namespace pl::modmenu {

/**
 * @brief Supported configuration entry kinds for Mod Menu modules.
 */
    enum class ConfigType {
        Toggle,
        SliderInt,
        SliderFloat,
        Radio,
        Color,
        Keybind,
        Text,
        Button,
    };

/**
 * @brief Interaction model used by an on-screen button.
 */
    enum class ButtonBehavior {
        Click,
        Hold,
        Toggle,
    };

/**
 * @brief Button event delivered to C++ callbacks.
 */
    enum class ButtonEvent {
        Click,
        Down,
        Up,
        StateChanged,
        Scroll,
    };

/**
 * @brief Built-in visual presets for on-screen buttons.
 */
    enum class ButtonStylePreset {
        Keycap,
        Accent,
    };

/**
 * @brief Encodings accepted for button icon data.
 */
    enum class ButtonIconFormat {
        Auto,
        Png,
        Webp,
        Svg,
        Resource,
    };

/**
 * @brief Primitive draw commands accepted by the HUD overlay.
 */
    enum class DrawCommandType {
        Text,
        Rect,
        Line,
        RectFilled,
        CircleFilled,
        TriangleFilled,
        Image,
    };

/**
 * @brief A single module configuration entry.
 */
    struct ConfigEntry {
        std::string key;
        std::string displayName;
        ConfigType type{};
        std::string defaultValue;
        std::string minValue;
        std::string maxValue;
        std::string dependsOn;
    };

/**
 * @brief Metadata and callbacks for a Mod Menu module.
 */
    struct ModuleInfo {
        std::string moduleId;
        std::string displayName;
        std::string description;
        std::string modId;
        bool defaultEnabled{};
        bool hideInHudEditor{};
        std::vector<ConfigEntry> configs;
        std::function<void(std::string_view moduleId, bool enabled)> onToggle;
        std::function<void(std::string_view moduleId, std::string_view key,
                std::string_view value)>
        onConfigChanged;
        std::function<void(std::string_view moduleId, std::string_view key,
                bool isDown)>
        onKeybind;
    };

/**
 * @brief Metadata and callbacks for an on-screen button.
 */
    struct ButtonInfo {
        std::string buttonId;
        std::string moduleId;
        std::string displayName;
        std::string modId;
        std::string label;
        int androidKeyCode{};
        ButtonBehavior behavior{ButtonBehavior::Click};
        bool defaultVisible{true};
        ButtonStylePreset stylePreset{ButtonStylePreset::Keycap};
        uint32_t normalBgColor{};
        uint32_t activeBgColor{};
        uint32_t borderColor{};
        uint32_t textColor{};
        uint32_t activeTextColor{};
        float widthScale{};
        float heightScale{1.0f};
        ButtonIconFormat iconFormat{ButtonIconFormat::Auto};
        bool hideLabelWhenIconPresent{true};
        std::vector<unsigned char> iconData;
        std::vector<unsigned char> activeIconData;
        std::function<void(std::string_view buttonId, ButtonEvent event, float value)>
        onEvent;
    };

/**
 * @brief A primitive draw command submitted by a module.
 */
    struct DrawCommand {
        DrawCommandType type{};
        float x{};
        float y{};
        float w{};
        float h{};
        float x3{};
        float y3{};
        uint32_t color{};
        float size{};
        std::string text;
        std::string fontId;
        std::string imageId;
    };

/**
 * @brief Registers a Mod Menu module.
 */
    PL_EXPORT bool registerModule(const ModuleInfo &info);

/**
 * @brief Unregisters a Mod Menu module and its owned callbacks.
 */
    PL_EXPORT void unregisterModule(std::string_view moduleId);

/**
 * @brief Updates a module enabled state.
 */
    PL_EXPORT void setModuleEnabled(std::string_view moduleId, bool enabled);

/**
 * @brief Replaces a module's current overlay draw commands.
 */
    PL_EXPORT void submitDrawCommands(std::string_view moduleId,
                                      std::span<const DrawCommand> commands);

/**
 * @brief Registers a TrueType font for overlay text rendering.
 */
    PL_EXPORT bool registerFont(std::string_view fontId,
                                std::span<const unsigned char> ttfData);

/**
 * @brief Registers raw RGBA image pixels for overlay image rendering.
 */
    PL_EXPORT bool registerImage(std::string_view imageId,
                                 std::span<const unsigned char> imageData,
                                 int width, int height);

/**
 * @brief Registers an on-screen button.
 */
    PL_EXPORT bool registerButton(const ButtonInfo &info);

/**
 * @brief Unregisters an on-screen button and its callback.
 */
    PL_EXPORT void unregisterButton(std::string_view buttonId);

/**
 * @brief Fluent helper for building and registering ModuleInfo.
 */
    class ModuleBuilder {
    public:
        ModuleBuilder(std::string moduleId, std::string displayName) {
            mInfo.moduleId = std::move(moduleId);
            mInfo.displayName = std::move(displayName);
        }

        ModuleBuilder &description(std::string value) {
            mInfo.description = std::move(value);
            return *this;
        }

        ModuleBuilder &modId(std::string value) {
            mInfo.modId = std::move(value);
            return *this;
        }

        ModuleBuilder &defaultEnabled(bool value) {
            mInfo.defaultEnabled = value;
            return *this;
        }

        ModuleBuilder &hideInHudEditor(bool value = true) {
            mInfo.hideInHudEditor = value;
            return *this;
        }

        ModuleBuilder &onToggle(
        std::function<void(std::string_view moduleId, bool enabled)> callback) {
            mInfo.onToggle = std::move(callback);
            return *this;
        }

        ModuleBuilder &onConfigChanged(
        std::function<void(std::string_view moduleId, std::string_view key,
                std::string_view value)>
        callback) {
            mInfo.onConfigChanged = std::move(callback);
            return *this;
        }

        ModuleBuilder &onKeybind(
        std::function<void(std::string_view moduleId, std::string_view key,
                bool isDown)>
        callback) {
            mInfo.onKeybind = std::move(callback);
            return *this;
        }

        ModuleBuilder &config(std::string key, std::string displayName,
                              ConfigType type, std::string defaultValue = {},
                              std::string minValue = {}, std::string maxValue = {},
                              std::string dependsOn = {}) {
            mInfo.configs.push_back(ConfigEntry{
                    .key = std::move(key),
                    .displayName = std::move(displayName),
                    .type = type,
                    .defaultValue = std::move(defaultValue),
                    .minValue = std::move(minValue),
                    .maxValue = std::move(maxValue),
                    .dependsOn = std::move(dependsOn),
            });
            return *this;
        }

        [[nodiscard]] bool registerModule() const {
            return pl::modmenu::registerModule(mInfo);
        }

    private:
        ModuleInfo mInfo;
    };

/**
 * @brief Fluent helper for building and registering ButtonInfo.
 */
    class ButtonBuilder {
    public:
        ButtonBuilder(std::string buttonId, std::string displayName) {
            mInfo.buttonId = std::move(buttonId);
            mInfo.displayName = std::move(displayName);
        }

        ButtonBuilder &moduleId(std::string value) {
            mInfo.moduleId = std::move(value);
            return *this;
        }

        ButtonBuilder &modId(std::string value) {
            mInfo.modId = std::move(value);
            return *this;
        }

        ButtonBuilder &label(std::string value) {
            mInfo.label = std::move(value);
            return *this;
        }

        ButtonBuilder &androidKeyCode(int value) {
            mInfo.androidKeyCode = value;
            return *this;
        }

        ButtonBuilder &behavior(ButtonBehavior value) {
            mInfo.behavior = value;
            return *this;
        }

        ButtonBuilder &defaultVisible(bool value) {
            mInfo.defaultVisible = value;
            return *this;
        }

        ButtonBuilder &stylePreset(ButtonStylePreset value) {
            mInfo.stylePreset = value;
            return *this;
        }

        ButtonBuilder &styleColors(uint32_t normalBgColor, uint32_t activeBgColor,
                                   uint32_t borderColor = 0) {
            mInfo.normalBgColor = normalBgColor;
            mInfo.activeBgColor = activeBgColor;
            mInfo.borderColor = borderColor;
            return *this;
        }

        ButtonBuilder &textColor(uint32_t value) {
            mInfo.textColor = value;
            return *this;
        }

        ButtonBuilder &activeTextColor(uint32_t value) {
            mInfo.activeTextColor = value;
            return *this;
        }

        ButtonBuilder &sizeScale(float width, float height = 1.0f) {
            mInfo.widthScale = width;
            mInfo.heightScale = height;
            return *this;
        }

        ButtonBuilder &icon(ButtonIconFormat format,
                            std::span<const unsigned char> data,
                            bool hideLabelWhenPresent = true) {
            mInfo.iconFormat = format;
            mInfo.hideLabelWhenIconPresent = hideLabelWhenPresent;
            mInfo.iconData.assign(data.begin(), data.end());
            return *this;
        }

        ButtonBuilder &pngIcon(std::span<const unsigned char> data,
                               bool hideLabelWhenPresent = true) {
            return icon(ButtonIconFormat::Png, data, hideLabelWhenPresent);
        }

        ButtonBuilder &webpIcon(std::span<const unsigned char> data,
                                bool hideLabelWhenPresent = true) {
            return icon(ButtonIconFormat::Webp, data, hideLabelWhenPresent);
        }

        ButtonBuilder &svgIcon(std::string svg, bool hideLabelWhenPresent = true) {
            mInfo.iconFormat = ButtonIconFormat::Svg;
            mInfo.hideLabelWhenIconPresent = hideLabelWhenPresent;
            mInfo.iconData.assign(svg.begin(), svg.end());
            return *this;
        }

        ButtonBuilder &resourceIcon(std::string resourceName, bool hideLabelWhenPresent = true) {
            mInfo.iconFormat = ButtonIconFormat::Resource;
            mInfo.hideLabelWhenIconPresent = hideLabelWhenPresent;
            mInfo.iconData.assign(resourceName.begin(), resourceName.end());
            return *this;
        }

        ButtonBuilder &activeSvgIcon(std::string svg) {
            mInfo.activeIconData.assign(svg.begin(), svg.end());
            return *this;
        }

        ButtonBuilder &onEvent(std::function<void(std::string_view buttonId,
        ButtonEvent event, float value)>
        callback) {
            mInfo.onEvent = std::move(callback);
            return *this;
        }

        [[nodiscard]] bool registerButton() const {
            return pl::modmenu::registerButton(mInfo);
        }

    private:
        ButtonInfo mInfo;
    };

} // namespace pl::modmenu
