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

typedef enum PLModMenu_ButtonBehavior {
  PL_BUTTON_CLICK = 0,
  PL_BUTTON_HOLD = 1,
  PL_BUTTON_TOGGLE = 2,
} PLModMenu_ButtonBehavior;

typedef enum PLModMenu_ButtonEvent {
  PL_BUTTON_EVENT_CLICK = 0,
  PL_BUTTON_EVENT_DOWN = 1,
  PL_BUTTON_EVENT_UP = 2,
  PL_BUTTON_EVENT_STATE_CHANGED = 3,
  PL_BUTTON_EVENT_SCROLL = 4,
} PLModMenu_ButtonEvent;

typedef void (*PLModMenu_OnButtonEvent_Fn)(const char *button_id,
                                           PLModMenu_ButtonEvent event,
                                           float value);

typedef enum PLModMenu_ButtonStylePreset {
  PL_BUTTON_STYLE_KEYCAP = 0,
  PL_BUTTON_STYLE_ACCENT = 1,
} PLModMenu_ButtonStylePreset;

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
  bool hide_in_hud_editor;
} PLModMenu_ModuleInfo;

typedef struct PLModMenu_ButtonInfo {
  const char *button_id;
  const char *module_id;
  const char *display_name;
  const char *mod_id;
  const char *label;
  int android_key_code;
  PLModMenu_ButtonBehavior behavior;
  bool default_visible;
  PLModMenu_OnButtonEvent_Fn on_event;
} PLModMenu_ButtonInfo;

typedef struct PLModMenu_ButtonStyle {
  PLModMenu_ButtonStylePreset preset;
  uint32_t normal_bg_color;
  uint32_t active_bg_color;
  uint32_t border_color;
  uint32_t text_color;
  uint32_t active_text_color;
} PLModMenu_ButtonStyle;

typedef struct PLModMenu_ButtonStyleV2 {
  PLModMenu_ButtonStyle base;
  float width_scale;
  float height_scale;
} PLModMenu_ButtonStyleV2;

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
  const char *font_id; /* for custom fonts */
} PLModMenu_DrawCommand;

typedef struct PLModMenu_Interface {
  bool (*RegisterModule)(const PLModMenu_ModuleInfo *info);
  void (*UnregisterModule)(const char *module_id);
  void (*SetModuleEnabled)(const char *module_id, bool enabled);
  void (*SubmitDrawCommands)(const char *module_id, const PLModMenu_DrawCommand *commands, int count);
  bool (*RegisterFont)(const char *font_id, const unsigned char *ttf_data, int ttf_size);
  bool (*RegisterButton)(const PLModMenu_ButtonInfo *info);
  void (*UnregisterButton)(const char *button_id);
  bool (*RegisterButtonWithStyle)(const PLModMenu_ButtonInfo *info,
                                  const PLModMenu_ButtonStyle *style);
  bool (*RegisterButtonWithStyleV2)(const PLModMenu_ButtonInfo *info,
                                    const PLModMenu_ButtonStyleV2 *style);
} PLModMenu_Interface;

PLAPI PLModMenu_Interface *GetPreloaderModMenu(void);

#ifdef __cplusplus
} // extern "C"
#endif
