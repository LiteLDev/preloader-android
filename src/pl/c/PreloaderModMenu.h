#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "pl/c/Macro.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*PLModMenu_OnToggle_Fn)(const char *module_id, bool enabled);
typedef void (*PLModMenu_OnConfigChanged_Fn)(const char *module_id,
                                             const char *key,
                                             const char *value);

typedef enum PLModMenu_ConfigType {
  PL_CONFIG_TOGGLE = 0,
  PL_CONFIG_SLIDER_INT,
  PL_CONFIG_SLIDER_FLOAT,
} PLModMenu_ConfigType;

typedef struct PLModMenu_ConfigEntry {
  const char *key;
  const char *display_name;
  PLModMenu_ConfigType type;
  const char *default_value;
  const char *min_value;
  const char *max_value;
} PLModMenu_ConfigEntry;

typedef struct PLModMenu_ModuleInfo {
  const char *module_id;
  const char *display_name;
  const char *description;
  const char *mod_id;
  bool default_enabled;
  PLModMenu_OnToggle_Fn on_toggle;
  int config_count;
  const PLModMenu_ConfigEntry *configs;
  PLModMenu_OnConfigChanged_Fn on_config_changed;
} PLModMenu_ModuleInfo;

typedef struct PLModMenu_Interface {
  bool (*RegisterModule)(const PLModMenu_ModuleInfo *info);
  void (*UnregisterModule)(const char *module_id);
  void (*SetModuleEnabled)(const char *module_id, bool enabled);
} PLModMenu_Interface;

PLAPI PLModMenu_Interface *GetPreloaderModMenu(void);

#ifdef __cplusplus
} // extern "C"
#endif
