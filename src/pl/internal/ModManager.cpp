#include "ModManager.h"
#include <dlfcn.h>
#include <fstream>
#include <nlohmann/json.hpp>

namespace {
bool EndsWithSo(const std::string &filename) {
  return filename.size() > 3 &&
         filename.compare(filename.size() - 3, 3, ".so") == 0;
}
} // namespace

bool ModManager::IsModEnabled(const std::filesystem::path &json_path,
                              const std::string &mod_filename) {
  if (!std::filesystem::exists(json_path))
    return false;
  std::ifstream json_file(json_path);
  if (!json_file)
    return false;
  nlohmann::json mods_config;
  try {
    json_file >> mods_config;
  } catch (...) {
    return false;
  }
  if (mods_config.is_object()) {
    auto it = mods_config.find(mod_filename);
    if (it != mods_config.end())
      return it.value().get<bool>();
  } else if (mods_config.is_array()) {
    for (auto &item : mods_config) {
      if (item.contains("name") && item["name"] == mod_filename) {
        return item.value("enabled", false);
      }
    }
  }
  return false;
}

using LoadFunc = void (*)(JavaVM *);

void ModManager::LoadAndInitializeEnabledMods(const std::string &modsDir,
                                              const std::string &dataDir,
                                              JavaVM *vm) {
  namespace fs = std::filesystem;
  fs::path modsPath(modsDir), configPath = modsPath / "mods_config.json";
  if (!fs::exists(modsPath))
    return;

  for (const auto &entry : fs::directory_iterator(modsPath)) {
    if (!entry.is_regular_file())
      continue;
    auto fname = entry.path().filename().string();
    if (!EndsWithSo(fname))
      continue;

    std::string libName = entry.path().stem().string() + ".so";
    if (!IsModEnabled(configPath, libName))
      continue;
    fs::path destPath = fs::path(dataDir) / "mods" / entry.path().filename();
    if (void *handle = dlopen(destPath.c_str(), RTLD_NOW)) {
      LoadFunc func = (LoadFunc)dlsym(handle, "LeviMod_Load");
      if (func) {
        func(vm);
      }
    }
  }
}
