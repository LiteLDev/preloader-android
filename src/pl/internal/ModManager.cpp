#include "ModManager.h"

#include <algorithm>
#include <dlfcn.h>
#include <string>
#include <utility>

#include "pl/Logger.hpp"
#include "pl/internal/LoadedModRegistry.h"
#include "pl/internal/ModManifest.h"
#include "pl/internal/NativeModLifecycle.h"

bool ModManager::LoadModLibrary(
    const std::filesystem::path &libraryPath,
    const std::optional<std::filesystem::path> &sourceModDirectory,
    JavaVM *vm) {
  using namespace pl::internal::mod;

  RuntimeModInfo modInfoStorage;
  if (!createRuntimeModInfo(libraryPath, sourceModDirectory, modInfoStorage)) {
    return false;
  }

  const std::string libraryPathString = libraryPath.lexically_normal().string();
  const std::string normalizedLibraryPath =
      normalizeLibraryPath(libraryPathString);

  if (isModAlreadyRegistered(normalizedLibraryPath)) {
    return true;
  }

  void *handle = acquireModHandle(libraryPathString);
  if (!handle) {
    const char *loadError = dlerror();
    preloaderLogger.error("Failed to load mod library {}: {}",
                          libraryPathString,
                          loadError ? loadError : "unknown error");
    return false;
  }

  auto entry = createLoadedModEntry(handle, vm, modInfoStorage,
                                    normalizedLibraryPath);
  if (!entry.has_value()) {
    return false;
  }

  registerLoadedMod(normalizedLibraryPath, std::move(*entry));
  return true;
}

void ModManager::EnableLoadedMods() {
  using namespace pl::internal::mod;

  const auto keys = getLoadedModKeysSnapshot();
  for (const auto &key : keys) {
    const auto entry = getLoadedModEntry(key);
    if (!entry.has_value() ||
        (entry->kind != LoadedModKind::Lifecycle &&
         entry->kind != LoadedModKind::CppLifecycle) ||
        entry->state != LoadedModState::Loaded) {
      continue;
    }

    if (enableLoadedModEntry(*entry)) {
      setLoadedModState(key, LoadedModState::Enabled);
    }
  }
}

void ModManager::DisableAndUnloadLoadedMods() {
  using namespace pl::internal::mod;

  auto keys = getLoadedModKeysSnapshot();
  std::reverse(keys.begin(), keys.end());

  for (const auto &key : keys) {
    auto entry = getLoadedModEntry(key);
    if (!entry.has_value() ||
        (entry->kind != LoadedModKind::Lifecycle &&
         entry->kind != LoadedModKind::CppLifecycle) ||
        entry->state == LoadedModState::Unloaded) {
      continue;
    }

    if (entry->state == LoadedModState::Enabled &&
        disableLoadedModEntry(*entry)) {
      setLoadedModState(key, LoadedModState::Loaded);
    }

    entry = getLoadedModEntry(key);
    if (!entry.has_value() || entry->state == LoadedModState::Unloaded) {
      continue;
    }

    if (unloadLoadedModEntry(*entry)) {
      setLoadedModState(key, LoadedModState::Unloaded);
    }
  }
}
