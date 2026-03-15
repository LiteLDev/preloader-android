#include "ModManager.h"

#include <algorithm>
#include <cstdint>
#include <dlfcn.h>
#include <fstream>
#include <optional>
#include <vector>

#include <nlohmann/json.hpp>

#include "pl/Logger.h"

namespace {
constexpr const char *kModLoadSymbol = "LeviMod_Load";
constexpr const char *kManifestFileName = "manifest.json";
constexpr const char *kPreloadNativeType = "preload-native";

auto &logger = preloader_logger;

struct ModConfigEntry {
  bool enabled = false;
  int32_t order = 0;
};

struct ParsedModDirectory {
  std::string id;
  std::string displayName;
  std::string author;
  std::string version;
  std::string entryPath;
  std::string entryFileName;
  std::string iconPath;
  std::filesystem::path rootPath;
  std::filesystem::path manifestPath;
};

struct PendingModLoad {
  std::filesystem::path libraryPath;
  int32_t order = 0;
};

struct RuntimeModInfoStorage {
  std::string modId;
  std::string displayName;
  std::string author;
  std::string version;
  std::string entryPath;
  std::string entryFileName;
  std::string libraryPath;
  std::string iconPath;
  std::string manifestPath;
  std::string modRootPath;
  PLModInfo info{};
};

bool EndsWithSo(const std::string &filename) {
  return filename.size() > 3 &&
         filename.compare(filename.size() - 3, 3, ".so") == 0;
}

bool IsSafeRelativePath(const std::filesystem::path &path) {
  if (path.empty() || path.is_absolute())
    return false;

  for (const auto &part : path) {
    const auto component = part.string();
    if (component.empty() || component == "." || component == "..")
      return false;
  }

  return true;
}

std::string GetOptionalString(const nlohmann::json &object, const char *key) {
  if (!object.contains(key) || !object[key].is_string())
    return {};
  return object[key].get<std::string>();
}

std::optional<ParsedModDirectory>
ParseModDirectory(const std::filesystem::path &modDirectory) {
  namespace fs = std::filesystem;

  const fs::path manifestPath = modDirectory / kManifestFileName;
  if (!fs::exists(manifestPath) || !fs::is_regular_file(manifestPath))
    return std::nullopt;

  std::ifstream manifestFile(manifestPath);
  if (!manifestFile)
    return std::nullopt;

  nlohmann::json manifestJson;
  try {
    manifestFile >> manifestJson;
  } catch (const std::exception &ex) {
    logger.warn("Failed to parse manifest {}: {}", manifestPath.string(),
                ex.what());
    return std::nullopt;
  }

  if (!manifestJson.is_object())
    return std::nullopt;

  const auto type = GetOptionalString(manifestJson, "type");
  if (type != kPreloadNativeType)
    return std::nullopt;

  const auto rawEntryPath = GetOptionalString(manifestJson, "entry");
  if (rawEntryPath.empty())
    return std::nullopt;

  std::string entryPath = rawEntryPath;
  std::replace(entryPath.begin(), entryPath.end(), '\\', '/');
  const fs::path relativeEntryPath(entryPath);
  if (!IsSafeRelativePath(relativeEntryPath)) {
    logger.warn("Rejected invalid mod entry path {} in {}", entryPath,
                manifestPath.string());
    return std::nullopt;
  }

  const fs::path entryFilePath = modDirectory / relativeEntryPath;
  if (!fs::exists(entryFilePath) || !fs::is_regular_file(entryFilePath)) {
    logger.warn("Mod entry {} declared in {} does not exist",
                entryFilePath.string(), manifestPath.string());
    return std::nullopt;
  }

  const auto entryFileName = entryFilePath.filename().string();
  if (!EndsWithSo(entryFileName)) {
    logger.warn("Mod entry {} is not a .so file", entryFilePath.string());
    return std::nullopt;
  }

  std::string iconPath = GetOptionalString(manifestJson, "icon");
  if (!iconPath.empty()) {
    std::replace(iconPath.begin(), iconPath.end(), '\\', '/');
    const fs::path relativeIconPath(iconPath);
    if (!IsSafeRelativePath(relativeIconPath) ||
        !fs::exists(modDirectory / relativeIconPath) ||
        !fs::is_regular_file(modDirectory / relativeIconPath)) {
      logger.warn("Ignoring invalid mod icon {} in {}", iconPath,
                  manifestPath.string());
      iconPath.clear();
    }
  }

  std::string displayName = GetOptionalString(manifestJson, "name");
  if (displayName.empty())
    displayName = modDirectory.filename().string();

  return ParsedModDirectory{
      .id = modDirectory.filename().string(),
      .displayName = std::move(displayName),
      .author = GetOptionalString(manifestJson, "author"),
      .version = GetOptionalString(manifestJson, "version"),
      .entryPath = entryPath,
      .entryFileName = entryFileName,
      .iconPath = std::move(iconPath),
      .rootPath = modDirectory,
      .manifestPath = manifestPath,
  };
}

std::optional<ModConfigEntry>
GetModConfigEntry(const std::filesystem::path &jsonPath,
                  const std::string &modId) {
  if (modId.empty() || !std::filesystem::exists(jsonPath))
    return std::nullopt;

  std::ifstream jsonFile(jsonPath);
  if (!jsonFile)
    return std::nullopt;

  nlohmann::json modsConfig;
  try {
    jsonFile >> modsConfig;
  } catch (...) {
    return std::nullopt;
  }

  if (modsConfig.is_object()) {
    auto it = modsConfig.find(modId);
    if (it != modsConfig.end() && it.value().is_boolean())
      return ModConfigEntry{it.value().get<bool>(), 0};
  } else if (modsConfig.is_array()) {
    int32_t fallbackOrder = 0;
    for (const auto &item : modsConfig) {
      if (!item.is_object() || !item.contains("name") ||
          !item["name"].is_string()) {
        ++fallbackOrder;
        continue;
      }

      if (item["name"].get<std::string>() != modId) {
        ++fallbackOrder;
        continue;
      }

      return ModConfigEntry{
          item.value("enabled", false),
          item.value("order", fallbackOrder),
      };
    }
  }

  return std::nullopt;
}

std::optional<std::filesystem::path>
FindModRootForLibraryPath(const std::filesystem::path &libraryPath) {
  namespace fs = std::filesystem;

  if (!fs::exists(libraryPath) || !fs::is_regular_file(libraryPath))
    return std::nullopt;

  for (auto current = libraryPath.parent_path(); !current.empty();
       current = current.parent_path()) {
    if (const auto parsed = ParseModDirectory(current); parsed.has_value())
      return current;

    const auto parent = current.parent_path();
    if (parent == current)
      break;
  }

  return std::nullopt;
}

void FinalizeRuntimeModInfo(RuntimeModInfoStorage &storage) {
  storage.info = PLModInfo{
      .size = sizeof(PLModInfo),
      .mod_id = storage.modId.c_str(),
      .display_name = storage.displayName.c_str(),
      .author = storage.author.c_str(),
      .version = storage.version.c_str(),
      .entry_path = storage.entryPath.c_str(),
      .entry_file_name = storage.entryFileName.c_str(),
      .library_path = storage.libraryPath.c_str(),
      .icon_path = storage.iconPath.c_str(),
      .manifest_path = storage.manifestPath.c_str(),
      .mod_root_path = storage.modRootPath.c_str(),
  };
}

bool CreateRuntimeModInfo(const std::filesystem::path &libraryPath,
                          RuntimeModInfoStorage &storage) {
  namespace fs = std::filesystem;

  const auto modRoot = FindModRootForLibraryPath(libraryPath);
  if (!modRoot.has_value()) {
    logger.error("Failed to resolve mod root for library {}",
                 libraryPath.string());
    return false;
  }

  const auto parsedMod = ParseModDirectory(*modRoot);
  if (!parsedMod.has_value()) {
    logger.error("Failed to parse mod manifest under {}", modRoot->string());
    return false;
  }

  const fs::path expectedLibraryPath =
      (parsedMod->rootPath / parsedMod->entryPath).lexically_normal();
  const fs::path requestedLibraryPath = libraryPath.lexically_normal();
  if (expectedLibraryPath != requestedLibraryPath) {
    logger.warn("Using manifest entry {} instead of requested library {}",
                expectedLibraryPath.string(), requestedLibraryPath.string());
  }

  if (!fs::exists(expectedLibraryPath) || !fs::is_regular_file(expectedLibraryPath)) {
    logger.error("Resolved mod library {} does not exist",
                 expectedLibraryPath.string());
    return false;
  }

  storage.modId = parsedMod->id;
  storage.displayName = parsedMod->displayName;
  storage.author = parsedMod->author;
  storage.version = parsedMod->version;
  storage.entryPath = parsedMod->entryPath;
  storage.entryFileName = parsedMod->entryFileName;
  storage.libraryPath = expectedLibraryPath.string();
  storage.iconPath = parsedMod->iconPath;
  storage.manifestPath = parsedMod->manifestPath.string();
  storage.modRootPath = parsedMod->rootPath.string();
  FinalizeRuntimeModInfo(storage);
  return true;
}

bool InvokeModEntry(void *handle, JavaVM *vm, const PLModInfo *modInfo) {
  if (auto load =
          reinterpret_cast<PLModLoadFunc>(dlsym(handle, kModLoadSymbol))) {
    load(vm, modInfo);
    return true;
  }

  return false;
}

} // namespace

bool ModManager::IsModEnabled(const std::filesystem::path &jsonPath,
                              const std::string &modId) {
  const auto configEntry = GetModConfigEntry(jsonPath, modId);
  return configEntry.has_value() && configEntry->enabled;
}

bool ModManager::LoadModLibrary(const std::filesystem::path &libraryPath,
                                JavaVM *vm) {
  RuntimeModInfoStorage modInfoStorage;
  if (!CreateRuntimeModInfo(libraryPath, modInfoStorage))
    return false;

  const std::string libraryPathString = modInfoStorage.libraryPath;
  void *handle = dlopen(libraryPathString.c_str(), RTLD_NOW);
  if (!handle) {
    logger.error("Failed to load mod library %s: %s", libraryPathString.c_str(),
                 dlerror());
    return false;
  }

  if (!InvokeModEntry(handle, vm, &modInfoStorage.info)) {
    logger.error("No supported mod entry found in %s",
                 libraryPathString.c_str());
    return false;
  }

  logger.info("Loaded mod library: %s", libraryPathString.c_str());
  return true;
}

void ModManager::LoadAndInitializeEnabledMods(const std::string &modsDir,
                                              const std::string &dataDir,
                                              JavaVM *vm) {
  namespace fs = std::filesystem;
  const fs::path modsPath(modsDir);
  const fs::path configPath = modsPath / "mods_config.json";
  if (!fs::exists(modsPath))
    return;

  std::vector<PendingModLoad> pendingLoads;

  for (const auto &entry : fs::directory_iterator(modsPath)) {
    if (!entry.is_directory())
      continue;

    const auto parsedMod = ParseModDirectory(entry.path());
    if (!parsedMod.has_value())
      continue;

    const auto configEntry = GetModConfigEntry(configPath, parsedMod->id);
    if (!configEntry.has_value() || !configEntry->enabled)
      continue;

    pendingLoads.push_back(PendingModLoad{
        .libraryPath = fs::path(dataDir) / "mods" / parsedMod->id /
                       parsedMod->entryPath,
        .order = configEntry->order,
    });
  }

  std::stable_sort(pendingLoads.begin(), pendingLoads.end(),
                   [](const PendingModLoad &lhs, const PendingModLoad &rhs) {
                     return lhs.order < rhs.order;
                   });

  for (const auto &pendingLoad : pendingLoads)
    LoadModLibrary(pendingLoad.libraryPath, vm);
}
