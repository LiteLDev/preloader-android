#include "ModManager.h"

#include <algorithm>
#include <android/log.h>
#include <cstdint>
#include <cstdio>
#include <dlfcn.h>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <vector>

#include <nlohmann/json.hpp>

#include "pl/Logger.h"
#include "pl/runtime/ModMenuBridge.h"

namespace {
constexpr const char *kModLoadSymbol = "LeviMod_Load";
constexpr const char *kLifecycleLoadSymbol = "PLMod_Load";
constexpr const char *kLifecycleEnableSymbol = "PLMod_Enable";
constexpr const char *kLifecycleDisableSymbol = "PLMod_Disable";
constexpr const char *kLifecycleUnloadSymbol = "PLMod_Unload";
constexpr const char *kManifestFileName = "manifest.json";
constexpr const char *kPreloadNativeType = "preload-native";
constexpr int kLoadedModLookupFlags = RTLD_NOLOAD | RTLD_NOW;
constexpr int kModDlopenFlags = RTLD_NOW | RTLD_GLOBAL;

auto &logger = preloader_logger;
std::mutex gInitializedModsMutex;

using PLModLifecycleLoadFunc = bool (*)(JavaVM *vm, const PLModInfo *mod_info);
using PLModLifecycleFunc = bool (*)();

enum class LoadedModKind {
  Legacy,
  Lifecycle,
  NoEntry,
};

enum class LoadedModState {
  Loaded,
  Enabled,
  Unloaded,
};

struct LoadedModEntry {
  std::string libraryPath;
  std::string modId;
  LoadedModKind kind{LoadedModKind::NoEntry};
  LoadedModState state{LoadedModState::Loaded};
  PLModLifecycleFunc enable{};
  PLModLifecycleFunc disable{};
  PLModLifecycleFunc unload{};
};

std::unordered_map<std::string, LoadedModEntry> gLoadedModLibraries;
std::vector<std::string> gLoadedModOrder;

void LogRaw(int priority, const char *format, const char *path,
            const char *detail = nullptr) {
  constexpr const char *kTag = "Preloader";
  if (detail) {
    __android_log_print(priority, kTag, format, path, detail);
    return;
  }

  __android_log_print(priority, kTag, format, path);
}

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

std::string NormalizeLibraryPath(const std::filesystem::path &libraryPath) {
  namespace fs = std::filesystem;

  std::error_code errorCode;
  const fs::path canonicalPath = fs::weakly_canonical(libraryPath, errorCode);
  if (!errorCode && !canonicalPath.empty())
    return canonicalPath.string();

  return libraryPath.lexically_normal().string();
}

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

std::optional<std::string> ReadTextFile(const std::filesystem::path &path) {
  FILE *file = std::fopen(path.string().c_str(), "rb");
  if (!file)
    return std::nullopt;

  std::string content;
  char buffer[4096];
  while (true) {
    const size_t bytesRead = std::fread(buffer, 1, sizeof(buffer), file);
    if (bytesRead > 0)
      content.append(buffer, bytesRead);

    if (bytesRead < sizeof(buffer)) {
      if (std::ferror(file)) {
        std::fclose(file);
        return std::nullopt;
      }
      break;
    }
  }

  std::fclose(file);
  return content;
}

std::optional<ParsedModDirectory>
ParseModDirectory(const std::filesystem::path &modDirectory) {
  namespace fs = std::filesystem;

  const fs::path manifestPath = modDirectory / kManifestFileName;
  if (!fs::exists(manifestPath) || !fs::is_regular_file(manifestPath))
    return std::nullopt;

  const auto manifestContent = ReadTextFile(manifestPath);
  if (!manifestContent.has_value())
    return std::nullopt;

  nlohmann::json manifestJson;
  try {
    manifestJson = nlohmann::json::parse(*manifestContent);
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

bool CreateRuntimeModInfo(
    const std::filesystem::path &libraryPath,
    const std::optional<std::filesystem::path> &sourceModDirectory,
    RuntimeModInfoStorage &storage) {
  namespace fs = std::filesystem;

  const auto modRoot = sourceModDirectory.has_value()
                           ? sourceModDirectory
                           : FindModRootForLibraryPath(libraryPath);
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

  if (!fs::exists(expectedLibraryPath) ||
      !fs::is_regular_file(expectedLibraryPath)) {
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

PLModLoadFunc ResolveLegacyModEntry(void *handle) {
  return reinterpret_cast<PLModLoadFunc>(dlsym(handle, kModLoadSymbol));
}

PLModLifecycleLoadFunc ResolveLifecycleLoad(void *handle) {
  return reinterpret_cast<PLModLifecycleLoadFunc>(
      dlsym(handle, kLifecycleLoadSymbol));
}

PLModLifecycleFunc ResolveLifecycleFunc(void *handle, const char *symbol) {
  return reinterpret_cast<PLModLifecycleFunc>(dlsym(handle, symbol));
}

bool IsModAlreadyRegistered(const std::string &normalizedLibraryPath) {
  std::lock_guard<std::mutex> lock(gInitializedModsMutex);
  const auto it = gLoadedModLibraries.find(normalizedLibraryPath);
  return it != gLoadedModLibraries.end() &&
         it->second.state != LoadedModState::Unloaded;
}

void RegisterLoadedMod(const std::string &normalizedLibraryPath,
                       LoadedModEntry entry) {
  std::lock_guard<std::mutex> lock(gInitializedModsMutex);
  const auto [it, inserted] =
      gLoadedModLibraries.emplace(normalizedLibraryPath, LoadedModEntry{});
  it->second = std::move(entry);
  if (inserted) {
    gLoadedModOrder.push_back(normalizedLibraryPath);
  }
}

std::vector<std::string> GetLoadedModKeysSnapshot() {
  std::lock_guard<std::mutex> lock(gInitializedModsMutex);
  std::vector<std::string> keys;
  keys.reserve(gLoadedModOrder.size());
  for (const auto &key : gLoadedModOrder) {
    const auto it = gLoadedModLibraries.find(key);
    if (it == gLoadedModLibraries.end())
      continue;

    const auto &entry = it->second;
    if (entry.kind == LoadedModKind::Lifecycle &&
        entry.state != LoadedModState::Unloaded) {
      keys.push_back(key);
    }
  }
  return keys;
}

std::optional<LoadedModEntry> GetLoadedModEntry(const std::string &key) {
  std::lock_guard<std::mutex> lock(gInitializedModsMutex);
  const auto it = gLoadedModLibraries.find(key);
  if (it == gLoadedModLibraries.end())
    return std::nullopt;

  return it->second;
}

void SetLoadedModState(const std::string &key, LoadedModState state) {
  std::lock_guard<std::mutex> lock(gInitializedModsMutex);
  const auto it = gLoadedModLibraries.find(key);
  if (it != gLoadedModLibraries.end()) {
    it->second.state = state;
  }
}

void *AcquireModHandle(const std::string &libraryPath) {
  if (void *handle = dlopen(libraryPath.c_str(), kLoadedModLookupFlags)) {
    return handle;
  }

  return dlopen(libraryPath.c_str(), kModDlopenFlags);
}

} // namespace

bool ModManager::LoadModLibrary(
    const std::filesystem::path &libraryPath,
    const std::optional<std::filesystem::path> &sourceModDirectory,
    JavaVM *vm) {
  RuntimeModInfoStorage modInfoStorage;
  if (!CreateRuntimeModInfo(libraryPath, sourceModDirectory, modInfoStorage))
    return false;

  const std::string libraryPathString = libraryPath.lexically_normal().string();
  const std::string normalizedLibraryPath =
      NormalizeLibraryPath(libraryPathString);

  if (IsModAlreadyRegistered(normalizedLibraryPath)) {
    return true;
  }

  void *handle = AcquireModHandle(libraryPathString);
  if (!handle) {
    LogRaw(ANDROID_LOG_ERROR, "Failed to load mod library %s: %s",
           libraryPathString.c_str(), dlerror());
    return false;
  }

  LoadedModEntry entry{
      .libraryPath = normalizedLibraryPath,
      .modId = modInfoStorage.modId,
      .kind = LoadedModKind::NoEntry,
      .state = LoadedModState::Loaded,
  };

  if (auto load = ResolveLifecycleLoad(handle)) {
    entry.kind = LoadedModKind::Lifecycle;
    entry.enable = ResolveLifecycleFunc(handle, kLifecycleEnableSymbol);
    entry.disable = ResolveLifecycleFunc(handle, kLifecycleDisableSymbol);
    entry.unload = ResolveLifecycleFunc(handle, kLifecycleUnloadSymbol);

    bool loaded = false;
    {
      pl::runtime::ScopedModMenuOwner owner(modInfoStorage.modId);
      loaded = load(vm, &modInfoStorage.info);
    }
    if (!loaded) {
      logger.error("Failed to run PLMod_Load for {}", modInfoStorage.modId);
      return false;
    }
  } else if (auto legacyLoad = ResolveLegacyModEntry(handle)) {
    entry.kind = LoadedModKind::Legacy;
    legacyLoad(vm, &modInfoStorage.info);
  } else {
    logger.warn("Mod library {} does not export a lifecycle entry",
                libraryPathString);
  }

  RegisterLoadedMod(normalizedLibraryPath, std::move(entry));

  return true;
}

void ModManager::EnableLoadedMods() {
  const auto keys = GetLoadedModKeysSnapshot();
  for (const auto &key : keys) {
    const auto entry = GetLoadedModEntry(key);
    if (!entry.has_value() || entry->kind != LoadedModKind::Lifecycle ||
        entry->state != LoadedModState::Loaded) {
      continue;
    }

    if (!entry->enable) {
      logger.warn("Lifecycle mod {} does not export {}", entry->modId,
                  kLifecycleEnableSymbol);
      continue;
    }

    bool enabled = false;
    {
      pl::runtime::ScopedModMenuOwner owner(entry->modId);
      enabled = entry->enable();
    }
    if (!enabled) {
      logger.error("Failed to enable lifecycle mod {}", entry->modId);
      continue;
    }

    SetLoadedModState(key, LoadedModState::Enabled);
  }
}

void ModManager::DisableAndUnloadLoadedMods() {
  auto keys = GetLoadedModKeysSnapshot();
  std::reverse(keys.begin(), keys.end());

  for (const auto &key : keys) {
    auto entry = GetLoadedModEntry(key);
    if (!entry.has_value() || entry->kind != LoadedModKind::Lifecycle ||
        entry->state == LoadedModState::Unloaded) {
      continue;
    }

    if (entry->state == LoadedModState::Enabled) {
      bool disabled = true;
      if (entry->disable) {
        {
          pl::runtime::ScopedModMenuOwner owner(entry->modId);
          disabled = entry->disable();
        }
        if (!disabled) {
          disabled = false;
          logger.error("Failed to disable lifecycle mod {}", entry->modId);
        }
      } else {
        logger.warn("Lifecycle mod {} does not export {}", entry->modId,
                    kLifecycleDisableSymbol);
      }
      if (disabled) {
        SetLoadedModState(key, LoadedModState::Loaded);
      }
    }

    entry = GetLoadedModEntry(key);
    if (!entry.has_value() || entry->state == LoadedModState::Unloaded) {
      continue;
    }

    if (entry->unload) {
      bool unloaded = false;
      {
        pl::runtime::ScopedModMenuOwner owner(entry->modId);
        unloaded = entry->unload();
      }
      if (!unloaded) {
        logger.error("Failed to unload lifecycle mod {}", entry->modId);
        continue;
      }
    } else {
      logger.warn("Lifecycle mod {} does not export {}", entry->modId,
                  kLifecycleUnloadSymbol);
    }
    SetLoadedModState(key, LoadedModState::Unloaded);
    pl::runtime::UnregisterModulesForModId(entry->modId);
  }
}
