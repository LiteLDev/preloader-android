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

} // namespace pl::modmenu
