#pragma once

#include "pl/c/PreloaderModMenu.h"

#include <string>
#include <vector>

namespace pl::runtime {

struct InternalDrawCommand {
  std::string module_id;
  PLModMenu_DrawCommandType type;
  float x, y, w, h;
  float x3, y3;
  uint32_t color;
  float size;
  std::string text;
  std::string font_id;
};

struct RegisteredModule {
  std::string module_id;
  std::string display_name;
  std::string description;
  std::string mod_id;
  bool enabled;
  bool hide_in_hud_editor;
  PLModMenu_OnToggle_Fn on_toggle;
  PLModMenu_OnConfigChanged_Fn on_config_changed;

  struct ConfigEntry {
    std::string key;
    std::string display_name;
    PLModMenu_ConfigType type;
    std::string default_value;
    std::string min_value;
    std::string max_value;
    std::string current_value;
    std::string depends_on;
  };
  std::vector<ConfigEntry> configs;
  std::vector<InternalDrawCommand> draw_commands;
};

PLModMenu_Interface *GetModMenuInterface();

class ScopedModMenuOwner {
public:
  explicit ScopedModMenuOwner(std::string modId);
  ~ScopedModMenuOwner();

  ScopedModMenuOwner(const ScopedModMenuOwner &) = delete;
  ScopedModMenuOwner &operator=(const ScopedModMenuOwner &) = delete;
};

int GetRegisteredModuleCount();
bool GetRegisteredModuleInfo(int index, RegisteredModule &out);
void ToggleRegisteredModule(const char *module_id, bool enabled);
void SetRegisteredModuleConfig(const char *module_id, const char *key,
                               const char *value);
void UnregisterModulesForModId(const std::string &modId);

void GetDrawCommands(std::vector<InternalDrawCommand> &out);

bool RegisterFontInternal(const char *font_id, const unsigned char *ttf_data,
                          int ttf_size);
bool GetRegisteredFontBytes(const char *font_id,
                            std::vector<unsigned char> &out);

} // namespace pl::runtime
