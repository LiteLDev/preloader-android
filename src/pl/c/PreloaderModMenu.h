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
  PL_CONFIG_RADIO,
  PL_CONFIG_COLOR,
} PLModMenu_ConfigType;

typedef struct PLModMenu_ConfigEntry {
  const char *key;
  const char *display_name;
  PLModMenu_ConfigType type;
  const char *default_value;
  const char *min_value;
  const char *max_value;
  const char *depends_on;
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

typedef enum PLModMenu_DrawCommandType {
  PL_DRAW_TEXT = 0,
  PL_DRAW_RECT = 1,
  PL_DRAW_LINE = 2,
  PL_DRAW_RECT_FILLED = 3,
  PL_DRAW_CIRCLE_FILLED = 4,
  PL_DRAW_TRIANGLE_FILLED = 5
} PLModMenu_DrawCommandType;

typedef struct PLModMenu_DrawCommand {
  PLModMenu_DrawCommandType type;
  float x, y, w, h;
  float x3, y3;
  uint32_t color; /* ARGB */
  float size; /* font size or line thickness */
  const char *text; /* for text */
} PLModMenu_DrawCommand;

typedef struct PLModMenu_Interface {
  bool (*RegisterModule)(const PLModMenu_ModuleInfo *info);
  void (*UnregisterModule)(const char *module_id);
  void (*SetModuleEnabled)(const char *module_id, bool enabled);
  void (*SubmitDrawCommands)(const char *module_id, const PLModMenu_DrawCommand *commands, int count);
} PLModMenu_Interface;

PLAPI PLModMenu_Interface *GetPreloaderModMenu(void);

#ifdef __cplusplus
} // extern "C"
#endif
