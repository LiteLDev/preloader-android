#include "ModManager.h"

#include <algorithm>
#include <android/log.h>
#include <cstdint>
#include <dlfcn.h>
#include <fstream>
#include <mutex>
#include <optional>
#include <unordered_set>
#include <vector>

#include <nlohmann/json.hpp>

#include "pl/Logger.h"

namespace {
constexpr const char *kModLoadSymbol = "LeviMod_Load";
constexpr const char *kManifestFileName = "manifest.json";
constexpr const char *kPreloadNativeType = "preload-native";
constexpr int kLoadedModLookupFlags = RTLD_NOLOAD | RTLD_NOW;
constexpr int kModDlopenFlags = RTLD_NOW | RTLD_GLOBAL;

auto &logger = preloader_logger;
std::mutex gInitializedModsMutex;
std::unordered_set<std::string> gInitializedModLibraries;

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

PLModLoadFunc ResolveModEntry(void *handle) {
  return reinterpret_cast<PLModLoadFunc>(dlsym(handle, kModLoadSymbol));
}

bool IsModAlreadyInitialized(const std::string &normalizedLibraryPath) {
  std::lock_guard<std::mutex> lock(gInitializedModsMutex);
  return gInitializedModLibraries.contains(normalizedLibraryPath);
}

void MarkModInitialized(const std::string &normalizedLibraryPath) {
  std::lock_guard<std::mutex> lock(gInitializedModsMutex);
  gInitializedModLibraries.insert(normalizedLibraryPath);
}

void *AcquireModHandle(const std::string &libraryPath) {
  if (void *handle = dlopen(libraryPath.c_str(), kLoadedModLookupFlags)) {
    return handle;
  }

  return dlopen(libraryPath.c_str(), kModDlopenFlags);
}

} // namespace

bool ModManager::LoadModLibrary(const std::filesystem::path &libraryPath,
                                JavaVM *vm) {
  RuntimeModInfoStorage modInfoStorage;
  if (!CreateRuntimeModInfo(libraryPath, modInfoStorage))
    return false;

  const std::string libraryPathString = modInfoStorage.libraryPath;
  const std::string normalizedLibraryPath =
      NormalizeLibraryPath(modInfoStorage.libraryPath);

  if (IsModAlreadyInitialized(normalizedLibraryPath)) {

    return true;
  }

  void *handle = AcquireModHandle(libraryPathString);
  if (!handle) {
    LogRaw(ANDROID_LOG_ERROR, "Failed to load mod library %s: %s",
           libraryPathString.c_str(), dlerror());
    return false;
  }

  if (auto load = ResolveModEntry(handle)) {
    load(vm, &modInfoStorage.info);
  }

  MarkModInitialized(normalizedLibraryPath);

  return true;
}
