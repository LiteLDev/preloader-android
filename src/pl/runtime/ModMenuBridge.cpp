#include "pl/runtime/ModMenuBridge.h"

#include <algorithm>
#include <mutex>
#include <vector>

namespace pl::runtime {
namespace {

std::vector<RegisteredModule> g_registeredModules;
std::mutex g_modMenuMutex;

bool RegisterModule(const PLModMenu_ModuleInfo *info) {
  if (!info || !info->module_id || !info->display_name)
    return false;

  std::lock_guard<std::mutex> lock(g_modMenuMutex);

  for (const auto &mod : g_registeredModules) {
    if (mod.module_id == info->module_id)
      return false;
  }

  RegisteredModule entry;
  entry.module_id = info->module_id;
  entry.display_name = info->display_name;
  entry.description = info->description ? info->description : "";
  entry.mod_id = info->mod_id ? info->mod_id : "";
  entry.enabled = info->default_enabled;
  entry.on_toggle = info->on_toggle;
  entry.on_config_changed = info->on_config_changed;

  for (int i = 0; i < info->config_count && info->configs; ++i) {
    const auto &src = info->configs[i];
    if (!src.key || !src.display_name)
      continue;
    RegisteredModule::ConfigEntry cfg;
    cfg.key = src.key;
    cfg.display_name = src.display_name;
    cfg.type = src.type;
    cfg.default_value = src.default_value ? src.default_value : "";
    cfg.min_value = src.min_value ? src.min_value : "";
    cfg.max_value = src.max_value ? src.max_value : "";
    cfg.current_value = cfg.default_value;
    cfg.depends_on = src.depends_on ? src.depends_on : "";
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
  if (!module_id)
    return;
  std::lock_guard<std::mutex> lock(g_modMenuMutex);
  for (auto &mod : g_registeredModules) {
    if (mod.module_id == module_id) {
      mod.enabled = enabled;
      if (mod.on_toggle)
        mod.on_toggle(module_id, enabled);
      return;
    }
  }
}

void SubmitDrawCommands(const char *module_id, const PLModMenu_DrawCommand *commands, int count) {
  if (!module_id) return;
  std::lock_guard<std::mutex> lock(g_modMenuMutex);
  for (auto &mod : g_registeredModules) {
    if (mod.module_id == module_id) {
      mod.draw_commands.clear();
      if (commands && count > 0) {
        for (int i = 0; i < count; ++i) {
          InternalDrawCommand icmd;
          icmd.module_id = module_id;
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
};

} // namespace

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
  PLModMenu_OnConfigChanged_Fn callback = nullptr;
  {
    std::lock_guard<std::mutex> lock(g_modMenuMutex);
    for (auto &mod : g_registeredModules) {
      if (mod.module_id == module_id) {
        for (auto &cfg : mod.configs) {
          if (cfg.key == key) {
            cfg.current_value = value ? value : "";
            break;
          }
        }
        callback = mod.on_config_changed;
        break;
      }
    }
  }
  if (callback)
    callback(module_id, key, value);
}

void GetDrawCommands(std::vector<InternalDrawCommand> &out) {
  std::lock_guard<std::mutex> lock(g_modMenuMutex);
  for (const auto &mod : g_registeredModules) {
    if (mod.enabled && !mod.draw_commands.empty()) {
      out.insert(out.end(), mod.draw_commands.begin(), mod.draw_commands.end());
    }
  }
}

} // namespace pl::runtime
