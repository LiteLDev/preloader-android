#include "pl/runtime/ModMenuBridge.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <mutex>
#include <utility>
#include <vector>

#include "pl/Logger.h"

namespace pl::runtime {
namespace {

constexpr size_t kMaxModuleIdLength = 160;
constexpr size_t kMaxDisplayNameLength = 160;
constexpr size_t kMaxDescriptionLength = 1024;
constexpr size_t kMaxModIdLength = 160;
constexpr size_t kMaxConfigStringLength = 2048;
constexpr size_t kMaxDrawTextLength = 4096;
constexpr int kMaxConfigCount = 128;
constexpr int kMaxDrawCommandCount = 4096;
constexpr int kMaxFontBytes = 8 * 1024 * 1024;

std::vector<RegisteredModule> g_registeredModules;
std::mutex g_modMenuMutex;
thread_local std::vector<std::string> g_currentOwnerModIds;

struct RegisteredFont {
  std::string font_id;
  std::vector<unsigned char> data;
};
std::vector<RegisteredFont> g_registeredFonts;

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
      preloader_logger.error("Mod Menu registration missing {}", fieldName);
    return !required;
  }

  const size_t length = strnlen(value, maxLength + 1);
  if (length == 0) {
    if (required)
      preloader_logger.error("Mod Menu registration has empty {}", fieldName);
    return !required;
  }
  if (length > maxLength) {
    preloader_logger.error("Mod Menu registration {} is too long", fieldName);
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

bool HasFiniteDrawGeometry(const PLModMenu_DrawCommand &command) {
  return std::isfinite(command.x) && std::isfinite(command.y) &&
         std::isfinite(command.w) && std::isfinite(command.h) &&
         std::isfinite(command.x3) && std::isfinite(command.y3) &&
         std::isfinite(command.size);
}

bool ValidateDrawCommands(const char *module_id,
                          const PLModMenu_DrawCommand *commands, int count) {
  if (count < 0 || count > kMaxDrawCommandCount) {
    preloader_logger.error("Rejected draw commands for {}: invalid count {}",
                           module_id, count);
    return false;
  }
  if (count > 0 && !commands) {
    preloader_logger.error("Rejected draw commands for {}: null command array",
                           module_id);
    return false;
  }

  for (int i = 0; i < count; ++i) {
    if (!HasFiniteDrawGeometry(commands[i])) {
      preloader_logger.error("Rejected draw command {} for {}: non-finite "
                             "geometry or size",
                             i, module_id);
      return false;
    }

    std::string unused;
    if (!ReadOptionalString(commands[i].text, kMaxDrawTextLength,
                            "draw command text", unused) ||
        !ReadOptionalString(commands[i].font_id, kMaxModuleIdLength,
                            "draw command font_id", unused)) {
      preloader_logger.error("Rejected draw command {} for {}", i, module_id);
      return false;
    }
  }
  return true;
}

bool RegisterModule(const PLModMenu_ModuleInfo *info) {
  if (!info) {
    preloader_logger.error("Mod Menu registration received null module info");
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
      preloader_logger.error(
          "Rejected Mod Menu module {}: declared mod_id {} does not match "
          "owning lifecycle mod {}",
          moduleId, modId, ownerModId);
      return false;
    }
  }

  if (info->config_count < 0 || info->config_count > kMaxConfigCount) {
    preloader_logger.error("Rejected Mod Menu module {}: invalid config_count "
                           "{}",
                           moduleId, info->config_count);
    return false;
  }
  if (info->config_count > 0 && !info->configs) {
    preloader_logger.error("Rejected Mod Menu module {}: null config array",
                           moduleId);
    return false;
  }

  std::lock_guard<std::mutex> lock(g_modMenuMutex);

  for (const auto &mod : g_registeredModules) {
    if (mod.module_id == moduleId) {
      preloader_logger.error("Rejected duplicate Mod Menu module {}",
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
      preloader_logger.error("Rejected Mod Menu module {}: config {} has "
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
      preloader_logger.error("Rejected Mod Menu module {}: invalid config {}",
                             entry.module_id, i);
      return false;
    }
    cfg.type = src.type;
    cfg.current_value = cfg.default_value;
    entry.configs.push_back(std::move(cfg));
  }

  g_registeredModules.push_back(std::move(entry));
  return true;
}

void UnregisterModule(const char *module_id) {
  if (!module_id)
    return;
  std::lock_guard<std::mutex> lock(g_modMenuMutex);
  g_registeredModules.erase(
      std::remove_if(g_registeredModules.begin(), g_registeredModules.end(),
                     [module_id](const RegisteredModule &m) {
                       return m.module_id == module_id;
                     }),
      g_registeredModules.end());
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
          mod.draw_commands.push_back(std::move(icmd));
        }
      }
      return;
    }
  }
}

PLModMenu_Interface g_modMenuInterface = {
    .RegisterModule = RegisterModule,
    .UnregisterModule = UnregisterModule,
    .SetModuleEnabled = SetModuleEnabled,
    .SubmitDrawCommands = SubmitDrawCommands,
    .RegisterFont = RegisterFontInternal,
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
  {
    std::lock_guard<std::mutex> lock(g_modMenuMutex);
    for (auto &mod : g_registeredModules) {
      if (mod.module_id == module_id) {
        mod.enabled = enabled;
        callback = mod.on_toggle;
        break;
      }
    }
  }
  if (callback)
    callback(module_id, enabled);
}

void SetRegisteredModuleConfig(const char *module_id, const char *key,
                               const char *value) {
  if (!module_id || !key)
    return;
  const char *safeValue = value ? value : "";
  PLModMenu_OnConfigChanged_Fn callback = nullptr;
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
        break;
      }
    }
  }
  if (callback)
    callback(module_id, key, safeValue);
}

void UnregisterModulesForModId(const std::string &modId) {
  if (modId.empty())
    return;

  std::lock_guard<std::mutex> lock(g_modMenuMutex);
  g_registeredModules.erase(
      std::remove_if(g_registeredModules.begin(), g_registeredModules.end(),
                     [&modId](const RegisteredModule &m) {
                       return m.mod_id == modId;
                     }),
      g_registeredModules.end());
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
    preloader_logger.error("Rejected registered font: invalid input or size "
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

} // namespace pl::runtime
