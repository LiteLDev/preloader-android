#pragma once

#include <string>
#include <utility>
#include <vector>

#include "pl/c/PreloaderModMenu.h"
#include "pl/cpp/mod/NativeMod.hpp"

namespace pl::modmenu {

class ModuleBuilder {
public:
  ModuleBuilder(std::string moduleId, std::string displayName)
      : moduleId(std::move(moduleId)), displayName(std::move(displayName)) {}

  ModuleBuilder &description(std::string value) {
    moduleDescription = std::move(value);
    return *this;
  }

  ModuleBuilder &modId(std::string value) {
    ownerModId = std::move(value);
    return *this;
  }

  ModuleBuilder &defaultEnabled(bool value) {
    enabledByDefault = value;
    return *this;
  }

  ModuleBuilder &onToggle(PLModMenu_OnToggle_Fn callback) {
    toggleCallback = callback;
    return *this;
  }

  ModuleBuilder &onConfigChanged(PLModMenu_OnConfigChanged_Fn callback) {
    configChangedCallback = callback;
    return *this;
  }

  ModuleBuilder &hideInHudEditor(bool value = true) {
    hiddenInHudEditor = value;
    return *this;
  }

  ModuleBuilder &config(std::string key, std::string displayName,
                        PLModMenu_ConfigType type,
                        std::string defaultValue = {},
                        std::string minValue = {},
                        std::string maxValue = {},
                        std::string dependsOn = {}) {
    configs.push_back(OwnedConfig{
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

  [[nodiscard]] bool registerModule(
      PLModMenu_Interface *menu = GetPreloaderModMenu()) const {
    if (!menu || !menu->RegisterModule)
      return false;

    rebuildConfigEntries();

    std::string resolvedModId = ownerModId;
    if (resolvedModId.empty()) {
      if (auto self = pl::mod::NativeMod::current()) {
        resolvedModId = self->getId();
      }
    }

    PLModMenu_ModuleInfo info{
        .module_id = moduleId.c_str(),
        .display_name = displayName.c_str(),
        .description = moduleDescription.empty() ? nullptr
                                                 : moduleDescription.c_str(),
        .mod_id = resolvedModId.empty() ? nullptr : resolvedModId.c_str(),
        .default_enabled = enabledByDefault,
        .on_toggle = toggleCallback,
        .config_count = static_cast<int>(configEntries.size()),
        .configs = configEntries.empty() ? nullptr : configEntries.data(),
        .on_config_changed = configChangedCallback,
        .hide_in_hud_editor = hiddenInHudEditor,
    };
    return menu->RegisterModule(&info);
  }

private:
  struct OwnedConfig {
    std::string key;
    std::string displayName;
    PLModMenu_ConfigType type{};
    std::string defaultValue;
    std::string minValue;
    std::string maxValue;
    std::string dependsOn;
  };

  void rebuildConfigEntries() const {
    configEntries.clear();
    configEntries.reserve(configs.size());
    for (const auto &config : configs) {
      configEntries.push_back(PLModMenu_ConfigEntry{
          .key = config.key.c_str(),
          .display_name = config.displayName.c_str(),
          .type = config.type,
          .default_value =
              config.defaultValue.empty() ? nullptr : config.defaultValue.c_str(),
          .min_value = config.minValue.empty() ? nullptr : config.minValue.c_str(),
          .max_value = config.maxValue.empty() ? nullptr : config.maxValue.c_str(),
          .depends_on = config.dependsOn.empty() ? nullptr : config.dependsOn.c_str(),
      });
    }
  }

  std::string moduleId;
  std::string displayName;
  std::string moduleDescription;
  std::string ownerModId;
  bool enabledByDefault{};
  bool hiddenInHudEditor{};
  PLModMenu_OnToggle_Fn toggleCallback{};
  PLModMenu_OnConfigChanged_Fn configChangedCallback{};
  std::vector<OwnedConfig> configs;
  mutable std::vector<PLModMenu_ConfigEntry> configEntries;
};

class ButtonBuilder {
public:
  ButtonBuilder(std::string buttonId, std::string displayName)
      : buttonId(std::move(buttonId)), displayName(std::move(displayName)) {}

  ButtonBuilder &moduleId(std::string value) {
    ownerModuleId = std::move(value);
    return *this;
  }

  ButtonBuilder &modId(std::string value) {
    ownerModId = std::move(value);
    return *this;
  }

  ButtonBuilder &label(std::string value) {
    buttonLabel = std::move(value);
    return *this;
  }

  ButtonBuilder &androidKeyCode(int value) {
    keyCode = value;
    return *this;
  }

  ButtonBuilder &behavior(PLModMenu_ButtonBehavior value) {
    buttonBehavior = value;
    return *this;
  }

  ButtonBuilder &defaultVisible(bool value) {
    visibleByDefault = value;
    return *this;
  }

  ButtonBuilder &stylePreset(PLModMenu_ButtonStylePreset value) {
    buttonStylePreset = value;
    return *this;
  }

  ButtonBuilder &styleColors(uint32_t normalBgColor, uint32_t activeBgColor,
                             uint32_t borderColor = 0) {
    normalBgColorValue = normalBgColor;
    activeBgColorValue = activeBgColor;
    borderColorValue = borderColor;
    return *this;
  }

  ButtonBuilder &textColor(uint32_t value) {
    textColorValue = value;
    return *this;
  }

  ButtonBuilder &activeTextColor(uint32_t value) {
    activeTextColorValue = value;
    return *this;
  }

  ButtonBuilder &sizeScale(float width, float height = 1.0f) {
    widthScale = width;
    heightScale = height;
    return *this;
  }

  ButtonBuilder &icon(PLModMenu_ButtonIconFormat format,
                      const unsigned char *data, int size,
                      bool hideLabelWhenPresent = true) {
    iconFormat = format;
    hideLabelWhenIconPresent = hideLabelWhenPresent;
    iconBytes.clear();
    if (data && size > 0) {
      iconBytes.assign(data, data + size);
    }
    return *this;
  }

  ButtonBuilder &pngIcon(const unsigned char *data, int size,
                         bool hideLabelWhenPresent = true) {
    return icon(PL_BUTTON_ICON_PNG, data, size, hideLabelWhenPresent);
  }

  ButtonBuilder &webpIcon(const unsigned char *data, int size,
                          bool hideLabelWhenPresent = true) {
    return icon(PL_BUTTON_ICON_WEBP, data, size, hideLabelWhenPresent);
  }

  ButtonBuilder &svgIcon(std::string svg,
                         bool hideLabelWhenPresent = true) {
    iconFormat = PL_BUTTON_ICON_SVG;
    hideLabelWhenIconPresent = hideLabelWhenPresent;
    iconBytes.assign(svg.begin(), svg.end());
    return *this;
  }

  ButtonBuilder &onEvent(PLModMenu_OnButtonEvent_Fn callback) {
    eventCallback = callback;
    return *this;
  }

  [[nodiscard]] bool registerButton(
      PLModMenu_Interface *menu = GetPreloaderModMenu()) const {
    if (!menu || !menu->RegisterButton)
      return false;

    std::string resolvedModId = ownerModId;
    if (resolvedModId.empty()) {
      if (auto self = pl::mod::NativeMod::current()) {
        resolvedModId = self->getId();
      }
    }

    PLModMenu_ButtonInfo info{
        .button_id = buttonId.c_str(),
        .module_id = ownerModuleId.c_str(),
        .display_name = displayName.c_str(),
        .mod_id = resolvedModId.empty() ? nullptr : resolvedModId.c_str(),
        .label = buttonLabel.empty() ? nullptr : buttonLabel.c_str(),
        .android_key_code = keyCode,
        .behavior = buttonBehavior,
        .default_visible = visibleByDefault,
        .on_event = eventCallback,
        .preset = buttonStylePreset,
        .normal_bg_color = normalBgColorValue,
        .active_bg_color = activeBgColorValue,
        .border_color = borderColorValue,
        .text_color = textColorValue,
        .active_text_color = activeTextColorValue,
        .width_scale = widthScale,
        .height_scale = heightScale,
        .icon_data = iconBytes.empty() ? nullptr : iconBytes.data(),
        .icon_data_size = static_cast<int>(iconBytes.size()),
        .icon_format = iconFormat,
        .hide_label_when_icon_present = hideLabelWhenIconPresent,
    };
    return menu->RegisterButton(&info);
  }

private:
  std::string buttonId;
  std::string ownerModuleId;
  std::string displayName;
  std::string ownerModId;
  std::string buttonLabel;
  int keyCode{};
  PLModMenu_ButtonBehavior buttonBehavior{PL_BUTTON_CLICK};
  bool visibleByDefault{true};
  float widthScale{};
  float heightScale{1.0f};
  PLModMenu_ButtonStylePreset buttonStylePreset{PL_BUTTON_STYLE_KEYCAP};
  uint32_t normalBgColorValue{};
  uint32_t activeBgColorValue{};
  uint32_t borderColorValue{};
  uint32_t textColorValue{};
  uint32_t activeTextColorValue{};
  PLModMenu_ButtonIconFormat iconFormat{PL_BUTTON_ICON_AUTO};
  bool hideLabelWhenIconPresent{true};
  std::vector<unsigned char> iconBytes;
  PLModMenu_OnButtonEvent_Fn eventCallback{};
};

} // namespace pl::modmenu
