#include "pl/runtime/ModMenuBridge.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <functional>
#include <limits>
#include <mutex>
#include <span>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include <lunasvg.h>

#include "pl/Logger.hpp"
#include "pl/ModMenu.hpp"

namespace pl::runtime {
namespace {

constexpr size_t kMaxModuleIdLength = 160;
constexpr size_t kMaxDisplayNameLength = 160;
constexpr size_t kMaxDescriptionLength = 1024;
constexpr size_t kMaxModIdLength = 160;
constexpr size_t kMaxButtonLabelLength = 32;
constexpr size_t kMaxConfigStringLength = 2048;
constexpr size_t kMaxDrawTextLength = 4096;
constexpr int kMaxConfigCount = 128;
constexpr int kMaxDrawCommandCount = 4096;
constexpr int kMaxFontBytes = 8 * 1024 * 1024;
constexpr int kMaxButtonIconBytes = 4 * 1024 * 1024;
constexpr int kDefaultRenderedIconSize = 128;
constexpr int kMaxRenderedIconSize = 512;
constexpr float kMinButtonWidthScale = 0.6f;
constexpr float kMaxButtonWidthScale = 4.0f;
constexpr float kMinButtonHeightScale = 0.6f;
constexpr float kMaxButtonHeightScale = 2.0f;

std::vector<RegisteredModule> g_registeredModules;
std::vector<RegisteredButton> g_registeredButtons;
std::mutex g_modMenuMutex;
thread_local std::vector<std::string> g_currentOwnerModIds;
std::unordered_map<std::string,
                   std::function<void(std::string_view, bool)>>
    g_cppToggleCallbacks;
std::unordered_map<std::string,
                   std::function<void(std::string_view, std::string_view,
                                      std::string_view)>>
    g_cppConfigChangedCallbacks;
std::unordered_map<std::string,
                   std::function<void(std::string_view,
                                      pl::modmenu::ButtonEvent, float)>>
    g_cppButtonEventCallbacks;

struct RegisteredFont {
  std::string font_id;
  std::vector<unsigned char> data;
};
std::vector<RegisteredFont> g_registeredFonts;

struct RegisteredImage {
  std::string image_id;
  std::vector<unsigned char> data;
  int width;
  int height;
};
std::vector<RegisteredImage> g_registeredImages;

std::string CurrentOwnerModId() {
  if (g_currentOwnerModIds.empty())
    return {};
  return g_currentOwnerModIds.back();
}

bool ReadString(const char *value, size_t maxLength, const char *fieldName,
                bool required, std::string &out) {
  out.clear();
  if (!value) {
    if (required)
      preloaderLogger.error("Mod Menu registration missing {}", fieldName);
    return !required;
  }

  const size_t length = strnlen(value, maxLength + 1);
  if (length == 0) {
    if (required)
      preloaderLogger.error("Mod Menu registration has empty {}", fieldName);
    return !required;
  }
  if (length > maxLength) {
    preloaderLogger.error("Mod Menu registration {} is too long", fieldName);
    return false;
  }

  out.assign(value, length);
  return true;
}

bool ReadRequiredString(const char *value, size_t maxLength,
                        const char *fieldName, std::string &out) {
  return ReadString(value, maxLength, fieldName, true, out);
}

bool ReadOptionalString(const char *value, size_t maxLength,
                        const char *fieldName, std::string &out) {
  return ReadString(value, maxLength, fieldName, false, out);
}

bool IsValidConfigType(PLModMenu_ConfigType type) {
  return type >= PL_CONFIG_TOGGLE && type <= PL_CONFIG_COLOR;
}

bool IsValidButtonBehavior(PLModMenu_ButtonBehavior behavior) {
  return behavior >= PL_BUTTON_CLICK && behavior <= PL_BUTTON_TOGGLE;
}

bool IsValidButtonStylePreset(PLModMenu_ButtonStylePreset preset) {
  return preset >= PL_BUTTON_STYLE_KEYCAP && preset <= PL_BUTTON_STYLE_ACCENT;
}

bool IsValidButtonIconFormat(PLModMenu_ButtonIconFormat format) {
  return format >= PL_BUTTON_ICON_AUTO && format <= PL_BUTTON_ICON_SVG;
}

bool IsValidButtonEvent(PLModMenu_ButtonEvent event) {
  return event >= PL_BUTTON_EVENT_CLICK && event <= PL_BUTTON_EVENT_SCROLL;
}

PLModMenu_ConfigType ToLegacyConfigType(pl::modmenu::ConfigType type) {
  switch (type) {
  case pl::modmenu::ConfigType::Toggle:
    return PL_CONFIG_TOGGLE;
  case pl::modmenu::ConfigType::SliderInt:
    return PL_CONFIG_SLIDER_INT;
  case pl::modmenu::ConfigType::SliderFloat:
    return PL_CONFIG_SLIDER_FLOAT;
  case pl::modmenu::ConfigType::Radio:
    return PL_CONFIG_RADIO;
  case pl::modmenu::ConfigType::Color:
    return PL_CONFIG_COLOR;
  }
  return PL_CONFIG_TOGGLE;
}

PLModMenu_ButtonBehavior
ToLegacyButtonBehavior(pl::modmenu::ButtonBehavior behavior) {
  switch (behavior) {
  case pl::modmenu::ButtonBehavior::Click:
    return PL_BUTTON_CLICK;
  case pl::modmenu::ButtonBehavior::Hold:
    return PL_BUTTON_HOLD;
  case pl::modmenu::ButtonBehavior::Toggle:
    return PL_BUTTON_TOGGLE;
  }
  return PL_BUTTON_CLICK;
}

PLModMenu_ButtonStylePreset
ToLegacyButtonStylePreset(pl::modmenu::ButtonStylePreset preset) {
  switch (preset) {
  case pl::modmenu::ButtonStylePreset::Keycap:
    return PL_BUTTON_STYLE_KEYCAP;
  case pl::modmenu::ButtonStylePreset::Accent:
    return PL_BUTTON_STYLE_ACCENT;
  }
  return PL_BUTTON_STYLE_KEYCAP;
}

PLModMenu_ButtonIconFormat
ToLegacyButtonIconFormat(pl::modmenu::ButtonIconFormat format) {
  switch (format) {
  case pl::modmenu::ButtonIconFormat::Auto:
    return PL_BUTTON_ICON_AUTO;
  case pl::modmenu::ButtonIconFormat::Png:
    return PL_BUTTON_ICON_PNG;
  case pl::modmenu::ButtonIconFormat::Webp:
    return PL_BUTTON_ICON_WEBP;
  case pl::modmenu::ButtonIconFormat::Svg:
    return PL_BUTTON_ICON_SVG;
  }
  return PL_BUTTON_ICON_AUTO;
}

PLModMenu_DrawCommandType
ToLegacyDrawCommandType(pl::modmenu::DrawCommandType type) {
  switch (type) {
  case pl::modmenu::DrawCommandType::Text:
    return PL_DRAW_TEXT;
  case pl::modmenu::DrawCommandType::Rect:
    return PL_DRAW_RECT;
  case pl::modmenu::DrawCommandType::Line:
    return PL_DRAW_LINE;
  case pl::modmenu::DrawCommandType::RectFilled:
    return PL_DRAW_RECT_FILLED;
  case pl::modmenu::DrawCommandType::CircleFilled:
    return PL_DRAW_CIRCLE_FILLED;
  case pl::modmenu::DrawCommandType::TriangleFilled:
    return PL_DRAW_TRIANGLE_FILLED;
  case pl::modmenu::DrawCommandType::Image:
    return PL_DRAW_IMAGE;
  }
  return PL_DRAW_TEXT;
}

pl::modmenu::ButtonEvent ToPublicButtonEvent(PLModMenu_ButtonEvent event) {
  switch (event) {
  case PL_BUTTON_EVENT_CLICK:
    return pl::modmenu::ButtonEvent::Click;
  case PL_BUTTON_EVENT_DOWN:
    return pl::modmenu::ButtonEvent::Down;
  case PL_BUTTON_EVENT_UP:
    return pl::modmenu::ButtonEvent::Up;
  case PL_BUTTON_EVENT_STATE_CHANGED:
    return pl::modmenu::ButtonEvent::StateChanged;
  case PL_BUTTON_EVENT_SCROLL:
    return pl::modmenu::ButtonEvent::Scroll;
  }
  return pl::modmenu::ButtonEvent::Click;
}

float NormalizeButtonScale(float value, float minValue, float maxValue) {
  if (!std::isfinite(value) || value <= 0.0f)
    return 0.0f;
  return std::clamp(value, minValue, maxValue);
}

PLModMenu_ButtonIconFormat
InferButtonIconFormat(const std::vector<unsigned char> &data,
                      PLModMenu_ButtonIconFormat declaredFormat) {
  if (declaredFormat != PL_BUTTON_ICON_AUTO)
    return declaredFormat;
  static constexpr unsigned char kPngSignature[] = {0x89, 'P', 'N', 'G',
                                                    0x0D, 0x0A, 0x1A, 0x0A};
  if (data.size() >= sizeof(kPngSignature) &&
      std::memcmp(data.data(), kPngSignature, sizeof(kPngSignature)) == 0) {
    return PL_BUTTON_ICON_PNG;
  }
  if (data.size() >= 12 &&
      std::memcmp(data.data(), "RIFF", 4) == 0 &&
      std::memcmp(data.data() + 8, "WEBP", 4) == 0) {
    return PL_BUTTON_ICON_WEBP;
  }

  size_t offset = 0;
  while (offset < data.size() &&
         std::isspace(static_cast<unsigned char>(data[offset]))) {
    ++offset;
  }
  if (offset < data.size() && data[offset] == '<') {
    return PL_BUTTON_ICON_SVG;
  }
  return PL_BUTTON_ICON_AUTO;
}

int NormalizeRenderedIconSize(int value) {
  if (value <= 0)
    return kDefaultRenderedIconSize;
  return std::clamp(value, 1, kMaxRenderedIconSize);
}

void AppendPngBytes(void *closure, void *data, int size) {
  if (!closure || !data || size <= 0)
    return;
  auto *out = static_cast<std::vector<unsigned char> *>(closure);
  const auto *bytes = static_cast<const unsigned char *>(data);
  out->insert(out->end(), bytes, bytes + size);
}

bool RenderSvgIconToPng(const std::vector<unsigned char> &svgData, int width,
                        int height, std::vector<unsigned char> &out) {
  auto document = lunasvg::Document::loadFromData(
      reinterpret_cast<const char *>(svgData.data()), svgData.size());
  if (!document) {
    preloaderLogger.error("Failed to parse registered SVG button icon");
    return false;
  }

  lunasvg::Bitmap bitmap = document->renderToBitmap(
      NormalizeRenderedIconSize(width), NormalizeRenderedIconSize(height),
      0x00000000);
  if (bitmap.isNull()) {
    preloaderLogger.error("Failed to render registered SVG button icon");
    return false;
  }

  out.clear();
  if (!bitmap.writeToPng(AppendPngBytes, &out)) {
    preloaderLogger.error("Failed to encode registered SVG button icon");
    return false;
  }
  return !out.empty();
}

bool HasFiniteDrawGeometry(const PLModMenu_DrawCommand &command) {
  return std::isfinite(command.x) && std::isfinite(command.y) &&
         std::isfinite(command.w) && std::isfinite(command.h) &&
         std::isfinite(command.x3) && std::isfinite(command.y3) &&
         std::isfinite(command.size);
}

bool ValidateDrawCommands(const char *module_id,
                          const PLModMenu_DrawCommand *commands, int count) {
  if (count < 0 || count > kMaxDrawCommandCount) {
    preloaderLogger.error("Rejected draw commands for {}: invalid count {}",
                           module_id, count);
    return false;
  }
  if (count > 0 && !commands) {
    preloaderLogger.error("Rejected draw commands for {}: null command array",
                           module_id);
    return false;
  }

  for (int i = 0; i < count; ++i) {
    if (!HasFiniteDrawGeometry(commands[i])) {
      preloaderLogger.error("Rejected draw command {} for {}: non-finite "
                             "geometry or size",
                             i, module_id);
      return false;
    }

    std::string unused;
    if (!ReadOptionalString(commands[i].text, kMaxDrawTextLength,
                            "draw command text", unused) ||
        !ReadOptionalString(commands[i].font_id, kMaxModuleIdLength,
                            "draw command font_id", unused) ||
        !ReadOptionalString(commands[i].image_id, kMaxModuleIdLength,
                            "draw command image_id", unused)) {
      preloaderLogger.error("Rejected draw command {} for {}", i, module_id);
      return false;
    }
  }
  return true;
}

bool RegisterModule(const PLModMenu_ModuleInfo *info) {
  if (!info) {
    preloaderLogger.error("Mod Menu registration received null module info");
    return false;
  }

  std::string moduleId;
  std::string displayName;
  std::string description;
  std::string modId;
  if (!ReadRequiredString(info->module_id, kMaxModuleIdLength, "module_id",
                          moduleId) ||
      !ReadRequiredString(info->display_name, kMaxDisplayNameLength,
                          "display_name", displayName) ||
      !ReadOptionalString(info->description, kMaxDescriptionLength,
                          "description", description) ||
      !ReadOptionalString(info->mod_id, kMaxModIdLength, "mod_id", modId)) {
    return false;
  }

  const std::string ownerModId = CurrentOwnerModId();
  if (!ownerModId.empty()) {
    if (modId.empty()) {
      modId = ownerModId;
    } else if (modId != ownerModId) {
      preloaderLogger.error(
          "Rejected Mod Menu module {}: declared mod_id {} does not match "
          "owning lifecycle mod {}",
          moduleId, modId, ownerModId);
      return false;
    }
  }

  if (info->config_count < 0 || info->config_count > kMaxConfigCount) {
    preloaderLogger.error("Rejected Mod Menu module {}: invalid config_count "
                           "{}",
                           moduleId, info->config_count);
    return false;
  }
  if (info->config_count > 0 && !info->configs) {
    preloaderLogger.error("Rejected Mod Menu module {}: null config array",
                           moduleId);
    return false;
  }

  std::lock_guard<std::mutex> lock(g_modMenuMutex);

  for (const auto &mod : g_registeredModules) {
    if (mod.module_id == moduleId) {
      preloaderLogger.error("Rejected duplicate Mod Menu module {}",
                             moduleId);
      return false;
    }
  }

  RegisteredModule entry;
  entry.module_id = std::move(moduleId);
  entry.display_name = std::move(displayName);
  entry.description = std::move(description);
  entry.mod_id = std::move(modId);
  entry.enabled = info->default_enabled;
  entry.hide_in_hud_editor = info->hide_in_hud_editor;
  entry.on_toggle = info->on_toggle;
  entry.on_config_changed = info->on_config_changed;
  if (info->config_count > 0)
    entry.configs.reserve(static_cast<size_t>(info->config_count));

  for (int i = 0; i < info->config_count; ++i) {
    const auto &src = info->configs[i];
    if (!IsValidConfigType(src.type)) {
      preloaderLogger.error("Rejected Mod Menu module {}: config {} has "
                             "invalid type {}",
                             entry.module_id, i, static_cast<int>(src.type));
      return false;
    }

    RegisteredModule::ConfigEntry cfg;
    if (!ReadRequiredString(src.key, kMaxConfigStringLength, "config key",
                            cfg.key) ||
        !ReadRequiredString(src.display_name, kMaxConfigStringLength,
                            "config display_name", cfg.display_name) ||
        !ReadOptionalString(src.default_value, kMaxConfigStringLength,
                            "config default_value", cfg.default_value) ||
        !ReadOptionalString(src.min_value, kMaxConfigStringLength,
                            "config min_value", cfg.min_value) ||
        !ReadOptionalString(src.max_value, kMaxConfigStringLength,
                            "config max_value", cfg.max_value) ||
        !ReadOptionalString(src.depends_on, kMaxConfigStringLength,
                            "config depends_on", cfg.depends_on)) {
      preloaderLogger.error("Rejected Mod Menu module {}: invalid config {}",
                             entry.module_id, i);
      return false;
    }
    cfg.type = src.type;
    cfg.current_value = cfg.default_value;
    entry.configs.push_back(std::move(cfg));
  }

  const std::string registeredModuleId = entry.module_id;
  const std::string registeredModId = entry.mod_id;
  const bool registeredEnabled = entry.enabled;
  g_registeredModules.push_back(std::move(entry));
  for (auto &button : g_registeredButtons) {
    if (button.module_id == registeredModuleId) {
      button.module_enabled = registeredEnabled;
      if (button.mod_id.empty())
        button.mod_id = registeredModId;
    }
  }
  return true;
}

void UnregisterModule(const char *module_id) {
  if (!module_id)
    return;
  std::lock_guard<std::mutex> lock(g_modMenuMutex);
  std::vector<std::string> removedButtons;
  for (const auto &button : g_registeredButtons) {
    if (button.module_id == module_id) {
      removedButtons.push_back(button.button_id);
    }
  }
  g_registeredModules.erase(
      std::remove_if(g_registeredModules.begin(), g_registeredModules.end(),
                     [module_id](const RegisteredModule &m) {
                       return m.module_id == module_id;
                     }),
      g_registeredModules.end());
  g_registeredButtons.erase(
      std::remove_if(g_registeredButtons.begin(), g_registeredButtons.end(),
                     [module_id](const RegisteredButton &button) {
                       return button.module_id == module_id;
                     }),
      g_registeredButtons.end());
  g_cppToggleCallbacks.erase(module_id);
  g_cppConfigChangedCallbacks.erase(module_id);
  for (const auto &buttonId : removedButtons) {
    g_cppButtonEventCallbacks.erase(buttonId);
  }
}

void SetModuleEnabled(const char *module_id, bool enabled) {
  ToggleRegisteredModule(module_id, enabled);
}

void SubmitDrawCommands(const char *module_id,
                        const PLModMenu_DrawCommand *commands, int count) {
  if (!module_id)
    return;
  std::string moduleId;
  if (!ReadRequiredString(module_id, kMaxModuleIdLength, "module_id",
                          moduleId) ||
      !ValidateDrawCommands(module_id, commands, count)) {
    return;
  }

  std::lock_guard<std::mutex> lock(g_modMenuMutex);
  for (auto &mod : g_registeredModules) {
    if (mod.module_id == moduleId) {
      mod.draw_commands.clear();
      if (commands && count > 0) {
        mod.draw_commands.reserve(static_cast<size_t>(count));
        for (int i = 0; i < count; ++i) {
          InternalDrawCommand icmd;
          icmd.module_id = moduleId;
          icmd.type = commands[i].type;
          icmd.x = commands[i].x;
          icmd.y = commands[i].y;
          icmd.w = commands[i].w;
          icmd.h = commands[i].h;
          icmd.x3 = commands[i].x3;
          icmd.y3 = commands[i].y3;
          icmd.color = commands[i].color;
          icmd.size = commands[i].size;
          if (commands[i].text) {
            icmd.text = commands[i].text;
          }
          if (commands[i].font_id) {
            icmd.font_id = commands[i].font_id;
          }
          if (commands[i].image_id) {
            icmd.image_id = commands[i].image_id;
          }
          mod.draw_commands.push_back(std::move(icmd));
        }
      }
      return;
    }
  }
}

bool RegisterButton(const PLModMenu_ButtonInfo *info) {
  if (!info) {
    preloaderLogger.error("Mod Menu button registration received null info");
    return false;
  }

  PLModMenu_ButtonStylePreset stylePreset = info->preset;
  if (!IsValidButtonStylePreset(stylePreset)) {
    preloaderLogger.error("Rejected Mod Menu button: invalid style preset {}",
                           static_cast<int>(stylePreset));
    return false;
  }

  if (!IsValidButtonIconFormat(info->icon_format)) {
    preloaderLogger.error("Rejected Mod Menu button: invalid icon format {}",
                           static_cast<int>(info->icon_format));
    return false;
  }

  std::vector<unsigned char> iconData;
  if (info->icon_data_size > 0) {
    if (!info->icon_data || info->icon_data_size > kMaxButtonIconBytes) {
      preloaderLogger.error("Rejected Mod Menu button: invalid icon size {}",
                             info->icon_data_size);
      return false;
    }
    iconData.assign(info->icon_data, info->icon_data + info->icon_data_size);
  } else if (info->icon_data_size < 0) {
    preloaderLogger.error("Rejected Mod Menu button: invalid icon size {}",
                           info->icon_data_size);
    return false;
  }

  const float resolvedWidthScale = NormalizeButtonScale(
      info->width_scale, kMinButtonWidthScale, kMaxButtonWidthScale);
  const float resolvedHeightScale =
      NormalizeButtonScale(info->height_scale, kMinButtonHeightScale,
                           kMaxButtonHeightScale);

  std::string buttonId;
  std::string moduleId;
  std::string displayName;
  std::string modId;
  std::string label;
  if (!ReadRequiredString(info->button_id, kMaxModuleIdLength, "button_id",
                          buttonId) ||
      !ReadRequiredString(info->module_id, kMaxModuleIdLength, "module_id",
                          moduleId) ||
      !ReadRequiredString(info->display_name, kMaxDisplayNameLength,
                          "button display_name", displayName) ||
      !ReadOptionalString(info->mod_id, kMaxModIdLength, "button mod_id",
                          modId) ||
      !ReadOptionalString(info->label, kMaxButtonLabelLength, "button label",
                          label)) {
    return false;
  }

  if (!IsValidButtonBehavior(info->behavior)) {
    preloaderLogger.error("Rejected Mod Menu button {}: invalid behavior {}",
                           buttonId, static_cast<int>(info->behavior));
    return false;
  }

  const std::string ownerModId = CurrentOwnerModId();
  if (!ownerModId.empty()) {
    if (modId.empty()) {
      modId = ownerModId;
    } else if (modId != ownerModId) {
      preloaderLogger.error(
          "Rejected Mod Menu button {}: declared mod_id {} does not match "
          "owning lifecycle mod {}",
          buttonId, modId, ownerModId);
      return false;
    }
  }

  std::lock_guard<std::mutex> lock(g_modMenuMutex);
  for (const auto &button : g_registeredButtons) {
    if (button.button_id == buttonId) {
      preloaderLogger.error("Rejected duplicate Mod Menu button {}",
                             buttonId);
      return false;
    }
  }

  bool moduleEnabled = false;
  for (const auto &mod : g_registeredModules) {
    if (mod.module_id == moduleId) {
      moduleEnabled = mod.enabled;
      if (modId.empty()) {
        modId = mod.mod_id;
      } else if (!mod.mod_id.empty() && modId != mod.mod_id) {
        preloaderLogger.error(
            "Rejected Mod Menu button {}: mod_id {} does not match module {} "
            "owner {}",
            buttonId, modId, moduleId, mod.mod_id);
        return false;
      }
      break;
    }
  }

  RegisteredButton entry;
  entry.button_id = std::move(buttonId);
  entry.module_id = std::move(moduleId);
  entry.display_name = std::move(displayName);
  entry.mod_id = std::move(modId);
  entry.label = std::move(label);
  entry.android_key_code = info->android_key_code;
  entry.behavior = info->behavior;
  entry.default_visible = info->default_visible;
  entry.module_enabled = moduleEnabled;
  entry.style_preset = stylePreset;
  entry.normal_bg_color = info->normal_bg_color;
  entry.active_bg_color = info->active_bg_color;
  entry.border_color = info->border_color;
  entry.text_color = info->text_color;
  entry.active_text_color = info->active_text_color;
  entry.width_scale = resolvedWidthScale;
  entry.height_scale = resolvedHeightScale;
  entry.icon_format = InferButtonIconFormat(iconData, info->icon_format);
  entry.hide_label_when_icon_present = info->hide_label_when_icon_present;
  entry.icon_data = std::move(iconData);
  entry.on_event = info->on_event;
  g_registeredButtons.push_back(std::move(entry));
  return true;
}

void UnregisterButton(const char *button_id) {
  if (!button_id)
    return;

  std::lock_guard<std::mutex> lock(g_modMenuMutex);
  g_registeredButtons.erase(
      std::remove_if(g_registeredButtons.begin(), g_registeredButtons.end(),
                     [button_id](const RegisteredButton &button) {
                       return button.button_id == button_id;
                     }),
      g_registeredButtons.end());
  g_cppButtonEventCallbacks.erase(button_id);
}

PLModMenu_Interface g_modMenuInterface = {
    .RegisterModule = RegisterModule,
    .UnregisterModule = UnregisterModule,
    .SetModuleEnabled = SetModuleEnabled,
    .SubmitDrawCommands = SubmitDrawCommands,
    .RegisterFont = RegisterFontInternal,
    .RegisterImage = RegisterImageInternal,
    .RegisterButton = RegisterButton,
    .UnregisterButton = UnregisterButton,
};

} // namespace

ScopedModMenuOwner::ScopedModMenuOwner(std::string modId) {
  g_currentOwnerModIds.push_back(std::move(modId));
}

ScopedModMenuOwner::~ScopedModMenuOwner() {
  if (!g_currentOwnerModIds.empty())
    g_currentOwnerModIds.pop_back();
}

PLModMenu_Interface *GetModMenuInterface() { return &g_modMenuInterface; }

int GetRegisteredModuleCount() {
  std::lock_guard<std::mutex> lock(g_modMenuMutex);
  return static_cast<int>(g_registeredModules.size());
}

bool GetRegisteredModuleInfo(int index, RegisteredModule &out) {
  std::lock_guard<std::mutex> lock(g_modMenuMutex);
  if (index < 0 || index >= static_cast<int>(g_registeredModules.size()))
    return false;
  out = g_registeredModules[index];
  return true;
}

void ToggleRegisteredModule(const char *module_id, bool enabled) {
  if (!module_id)
    return;
  PLModMenu_OnToggle_Fn callback = nullptr;
  std::function<void(std::string_view, bool)> cppCallback;
  {
    std::lock_guard<std::mutex> lock(g_modMenuMutex);
    for (auto &mod : g_registeredModules) {
      if (mod.module_id == module_id) {
        mod.enabled = enabled;
        callback = mod.on_toggle;
        if (const auto it = g_cppToggleCallbacks.find(mod.module_id);
            it != g_cppToggleCallbacks.end()) {
          cppCallback = it->second;
        }
        break;
      }
    }
    for (auto &button : g_registeredButtons) {
      if (button.module_id == module_id)
        button.module_enabled = enabled;
    }
  }
  if (callback)
    callback(module_id, enabled);
  if (cppCallback)
    cppCallback(module_id, enabled);
}

void SetRegisteredModuleConfig(const char *module_id, const char *key,
                               const char *value) {
  if (!module_id || !key)
    return;
  const char *safeValue = value ? value : "";
  PLModMenu_OnConfigChanged_Fn callback = nullptr;
  std::function<void(std::string_view, std::string_view, std::string_view)>
      cppCallback;
  {
    std::lock_guard<std::mutex> lock(g_modMenuMutex);
    for (auto &mod : g_registeredModules) {
      if (mod.module_id == module_id) {
        for (auto &cfg : mod.configs) {
          if (cfg.key == key) {
            cfg.current_value = safeValue;
            break;
          }
        }
        callback = mod.on_config_changed;
        if (const auto it = g_cppConfigChangedCallbacks.find(mod.module_id);
            it != g_cppConfigChangedCallbacks.end()) {
          cppCallback = it->second;
        }
        break;
      }
    }
  }
  if (callback)
    callback(module_id, key, safeValue);
  if (cppCallback)
    cppCallback(module_id, key, safeValue);
}

void UnregisterModulesForModId(const std::string &modId) {
  if (modId.empty())
    return;

  std::lock_guard<std::mutex> lock(g_modMenuMutex);
  std::vector<std::string> removedModules;
  std::vector<std::string> removedButtons;
  for (const auto &module : g_registeredModules) {
    if (module.mod_id == modId) {
      removedModules.push_back(module.module_id);
    }
  }
  for (const auto &button : g_registeredButtons) {
    if (button.mod_id == modId) {
      removedButtons.push_back(button.button_id);
    }
  }
  g_registeredModules.erase(
      std::remove_if(g_registeredModules.begin(), g_registeredModules.end(),
                     [&modId](const RegisteredModule &m) {
                       return m.mod_id == modId;
                     }),
      g_registeredModules.end());
  g_registeredButtons.erase(
      std::remove_if(g_registeredButtons.begin(), g_registeredButtons.end(),
                     [&modId](const RegisteredButton &button) {
                       return button.mod_id == modId;
                     }),
      g_registeredButtons.end());
  for (const auto &moduleId : removedModules) {
    g_cppToggleCallbacks.erase(moduleId);
    g_cppConfigChangedCallbacks.erase(moduleId);
  }
  for (const auto &buttonId : removedButtons) {
    g_cppButtonEventCallbacks.erase(buttonId);
  }
}

int GetRegisteredButtonCount() {
  std::lock_guard<std::mutex> lock(g_modMenuMutex);
  return static_cast<int>(g_registeredButtons.size());
}

bool GetRegisteredButtonInfo(int index, RegisteredButton &out) {
  std::lock_guard<std::mutex> lock(g_modMenuMutex);
  if (index < 0 || index >= static_cast<int>(g_registeredButtons.size()))
    return false;

  out = g_registeredButtons[index];
  for (const auto &mod : g_registeredModules) {
    if (mod.module_id == out.module_id) {
      out.module_enabled = mod.enabled;
      break;
    }
  }
  return true;
}

bool GetRegisteredButtonIconBytes(const char *button_id, int width, int height,
                                  std::vector<unsigned char> &out) {
  if (!button_id)
    return false;

  std::vector<unsigned char> iconData;
  PLModMenu_ButtonIconFormat iconFormat = PL_BUTTON_ICON_AUTO;
  {
    std::lock_guard<std::mutex> lock(g_modMenuMutex);
    for (const auto &button : g_registeredButtons) {
      if (button.button_id == button_id) {
        iconData = button.icon_data;
        iconFormat = button.icon_format;
        break;
      }
    }
  }

  if (iconData.empty())
    return false;

  iconFormat = InferButtonIconFormat(iconData, iconFormat);
  if (iconFormat == PL_BUTTON_ICON_SVG) {
    return RenderSvgIconToPng(iconData, width, height, out);
  }

  out = std::move(iconData);
  return !out.empty();
}

void DispatchRegisteredButtonEvent(const char *button_id,
                                   PLModMenu_ButtonEvent event, float value) {
  if (!button_id || !IsValidButtonEvent(event))
    return;

  PLModMenu_OnButtonEvent_Fn callback = nullptr;
  std::function<void(std::string_view, pl::modmenu::ButtonEvent, float)>
      cppCallback;
  {
    std::lock_guard<std::mutex> lock(g_modMenuMutex);
    for (const auto &button : g_registeredButtons) {
      if (button.button_id == button_id) {
        callback = button.on_event;
        if (const auto it = g_cppButtonEventCallbacks.find(button.button_id);
            it != g_cppButtonEventCallbacks.end()) {
          cppCallback = it->second;
        }
        break;
      }
    }
  }
  if (callback)
    callback(button_id, event, value);
  if (cppCallback)
    cppCallback(button_id, ToPublicButtonEvent(event), value);
}

void GetDrawCommands(std::vector<InternalDrawCommand> &out) {
  std::lock_guard<std::mutex> lock(g_modMenuMutex);
  size_t commandCount = 0;
  for (const auto &mod : g_registeredModules) {
    if (mod.enabled)
      commandCount += mod.draw_commands.size();
  }
  out.reserve(out.size() + commandCount);
  for (const auto &mod : g_registeredModules) {
    if (mod.enabled && !mod.draw_commands.empty()) {
      out.insert(out.end(), mod.draw_commands.begin(), mod.draw_commands.end());
    }
  }
}

bool RegisterFontInternal(const char *font_id, const unsigned char *ttf_data,
                          int ttf_size) {
  std::string fontId;
  if (!ReadRequiredString(font_id, kMaxModuleIdLength, "font_id", fontId) ||
      !ttf_data || ttf_size <= 0 || ttf_size > kMaxFontBytes) {
    preloaderLogger.error("Rejected registered font: invalid input or size "
                           "{}",
                           ttf_size);
    return false;
  }
  std::lock_guard<std::mutex> lock(g_modMenuMutex);
  for (auto &f : g_registeredFonts) {
    if (f.font_id == fontId)
      return false; // Already registered
  }
  RegisteredFont f;
  f.font_id = std::move(fontId);
  f.data.assign(ttf_data, ttf_data + ttf_size);
  g_registeredFonts.push_back(std::move(f));
  return true;
}

bool GetRegisteredFontBytes(const char *font_id,
                            std::vector<unsigned char> &out) {
  if (!font_id)
    return false;
  std::lock_guard<std::mutex> lock(g_modMenuMutex);
  for (const auto &f : g_registeredFonts) {
    if (f.font_id == font_id) {
      out = f.data;
      return true;
    }
  }
  return false;
}

bool RegisterImageInternal(const char *image_id, const unsigned char *image_data,
                           int width, int height) {
  std::string imageId;
  if (!ReadRequiredString(image_id, kMaxModuleIdLength, "image_id", imageId) ||
      width <= 0 || height <= 0 || !image_data) {
    return false;
  }

  const auto imageWidth = static_cast<size_t>(width);
  const auto imageHeight = static_cast<size_t>(height);
  const auto maxInt = static_cast<size_t>(std::numeric_limits<int>::max());
  if (imageWidth > maxInt / imageHeight) {
    return false;
  }
  const size_t pixelCount = imageWidth * imageHeight;
  if (pixelCount > maxInt / 4) {
    return false;
  }

  const size_t byteCount = pixelCount * 4;
  std::lock_guard<std::mutex> lock(g_modMenuMutex);
  for (const auto &image : g_registeredImages) {
    if (image.image_id == imageId) {
      return false;
    }
  }

  RegisteredImage image;
  image.image_id = std::move(imageId);
  image.data.assign(image_data, image_data + byteCount);
  image.width = width;
  image.height = height;
  g_registeredImages.push_back(std::move(image));
  return true;
}

bool GetRegisteredImageBytes(const char *image_id,
                             std::vector<unsigned char> &out, int &width, int &height) {
  if (!image_id) {
    return false;
  }
  std::lock_guard<std::mutex> lock(g_modMenuMutex);
  for (const auto &image : g_registeredImages) {
    if (image.image_id == image_id) {
      out = image.data;
      width = image.width;
      height = image.height;
      return true;
    }
  }
  return false;
}

bool RegisterCppModule(const pl::modmenu::ModuleInfo &info) {
  std::vector<PLModMenu_ConfigEntry> configs;
  configs.reserve(info.configs.size());
  for (const auto &config : info.configs) {
    configs.push_back(PLModMenu_ConfigEntry{
        .key = config.key.c_str(),
        .display_name = config.displayName.c_str(),
        .type = ToLegacyConfigType(config.type),
        .default_value =
            config.defaultValue.empty() ? nullptr : config.defaultValue.c_str(),
        .min_value = config.minValue.empty() ? nullptr : config.minValue.c_str(),
        .max_value = config.maxValue.empty() ? nullptr : config.maxValue.c_str(),
        .depends_on =
            config.dependsOn.empty() ? nullptr : config.dependsOn.c_str(),
    });
  }

  if (info.configs.size() >
      static_cast<size_t>(std::numeric_limits<int>::max())) {
    return false;
  }

  const PLModMenu_ModuleInfo legacyInfo{
      .module_id = info.moduleId.c_str(),
      .display_name = info.displayName.c_str(),
      .description = info.description.empty() ? nullptr
                                              : info.description.c_str(),
      .mod_id = info.modId.empty() ? nullptr : info.modId.c_str(),
      .default_enabled = info.defaultEnabled,
      .on_toggle = nullptr,
      .config_count = static_cast<int>(configs.size()),
      .configs = configs.empty() ? nullptr : configs.data(),
      .on_config_changed = nullptr,
      .hide_in_hud_editor = info.hideInHudEditor,
  };

  if (!RegisterModule(&legacyInfo)) {
    return false;
  }

  std::lock_guard<std::mutex> lock(g_modMenuMutex);
  if (info.onToggle) {
    g_cppToggleCallbacks[info.moduleId] = info.onToggle;
  }
  if (info.onConfigChanged) {
    g_cppConfigChangedCallbacks[info.moduleId] = info.onConfigChanged;
  }
  return true;
}

bool RegisterCppButton(const pl::modmenu::ButtonInfo &info) {
  if (info.iconData.size() >
      static_cast<size_t>(std::numeric_limits<int>::max())) {
    return false;
  }

  const PLModMenu_ButtonInfo legacyInfo{
      .button_id = info.buttonId.c_str(),
      .module_id = info.moduleId.c_str(),
      .display_name = info.displayName.c_str(),
      .mod_id = info.modId.empty() ? nullptr : info.modId.c_str(),
      .label = info.label.empty() ? nullptr : info.label.c_str(),
      .android_key_code = info.androidKeyCode,
      .behavior = ToLegacyButtonBehavior(info.behavior),
      .default_visible = info.defaultVisible,
      .on_event = nullptr,
      .preset = ToLegacyButtonStylePreset(info.stylePreset),
      .normal_bg_color = info.normalBgColor,
      .active_bg_color = info.activeBgColor,
      .border_color = info.borderColor,
      .text_color = info.textColor,
      .active_text_color = info.activeTextColor,
      .width_scale = info.widthScale,
      .height_scale = info.heightScale,
      .icon_data = info.iconData.empty() ? nullptr : info.iconData.data(),
      .icon_data_size = static_cast<int>(info.iconData.size()),
      .icon_format = ToLegacyButtonIconFormat(info.iconFormat),
      .hide_label_when_icon_present = info.hideLabelWhenIconPresent,
  };

  if (!RegisterButton(&legacyInfo)) {
    return false;
  }

  std::lock_guard<std::mutex> lock(g_modMenuMutex);
  if (info.onEvent) {
    g_cppButtonEventCallbacks[info.buttonId] = info.onEvent;
  }
  return true;
}

void SubmitCppDrawCommands(std::string_view moduleId,
                           std::span<const pl::modmenu::DrawCommand> commands) {
  if (commands.size() > static_cast<size_t>(std::numeric_limits<int>::max())) {
    return;
  }

  std::vector<PLModMenu_DrawCommand> legacyCommands;
  legacyCommands.reserve(commands.size());
  for (const auto &command : commands) {
    legacyCommands.push_back(PLModMenu_DrawCommand{
        .type = ToLegacyDrawCommandType(command.type),
        .x = command.x,
        .y = command.y,
        .w = command.w,
        .h = command.h,
        .x3 = command.x3,
        .y3 = command.y3,
        .color = command.color,
        .size = command.size,
        .text = command.text.empty() ? nullptr : command.text.c_str(),
        .font_id = command.fontId.empty() ? nullptr : command.fontId.c_str(),
        .image_id =
            command.imageId.empty() ? nullptr : command.imageId.c_str(),
    });
  }

  const std::string moduleIdString(moduleId);
  SubmitDrawCommands(moduleIdString.c_str(),
                     legacyCommands.empty() ? nullptr : legacyCommands.data(),
                     static_cast<int>(legacyCommands.size()));
}

bool RegisterCppFont(std::string_view fontId,
                     std::span<const unsigned char> ttfData) {
  if (ttfData.size() > static_cast<size_t>(std::numeric_limits<int>::max())) {
    return false;
  }
  const std::string fontIdString(fontId);
  return RegisterFontInternal(fontIdString.c_str(), ttfData.data(),
                              static_cast<int>(ttfData.size()));
}

bool RegisterCppImage(std::string_view imageId,
                      std::span<const unsigned char> imageData, int width,
                      int height) {
  if (width <= 0 || height <= 0) {
    return false;
  }

  const auto imageWidth = static_cast<size_t>(width);
  const auto imageHeight = static_cast<size_t>(height);
  const auto maxInt = static_cast<size_t>(std::numeric_limits<int>::max());
  if (imageWidth > maxInt / imageHeight) {
    return false;
  }
  const size_t pixelCount = imageWidth * imageHeight;
  if (pixelCount > maxInt / 4) {
    return false;
  }

  const size_t byteCount = pixelCount * 4;
  if (imageData.size() != byteCount) {
    return false;
  }

  const std::string imageIdString(imageId);
  return RegisterImageInternal(imageIdString.c_str(), imageData.data(), width,
                               height);
}

} // namespace pl::runtime

namespace pl::modmenu {

bool registerModule(const ModuleInfo &info) {
  return pl::runtime::RegisterCppModule(info);
}

void unregisterModule(std::string_view moduleId) {
  const std::string moduleIdString(moduleId);
  pl::runtime::GetModMenuInterface()->UnregisterModule(moduleIdString.c_str());
}

void setModuleEnabled(std::string_view moduleId, bool enabled) {
  const std::string moduleIdString(moduleId);
  pl::runtime::GetModMenuInterface()->SetModuleEnabled(moduleIdString.c_str(),
                                                       enabled);
}

void submitDrawCommands(std::string_view moduleId,
                        std::span<const DrawCommand> commands) {
  pl::runtime::SubmitCppDrawCommands(moduleId, commands);
}

bool registerFont(std::string_view fontId,
                  std::span<const unsigned char> ttfData) {
  return pl::runtime::RegisterCppFont(fontId, ttfData);
}

bool registerImage(std::string_view imageId,
                   std::span<const unsigned char> imageData, int width,
                   int height) {
  return pl::runtime::RegisterCppImage(imageId, imageData, width, height);
}

bool registerButton(const ButtonInfo &info) {
  return pl::runtime::RegisterCppButton(info);
}

void unregisterButton(std::string_view buttonId) {
  const std::string buttonIdString(buttonId);
  pl::runtime::GetModMenuInterface()->UnregisterButton(buttonIdString.c_str());
}

} // namespace pl::modmenu
