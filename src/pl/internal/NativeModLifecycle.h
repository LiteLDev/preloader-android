#pragma once

#include <jni.h>

#include <optional>
#include <string>

#include "pl/internal/LoadedModRegistry.h"
#include "pl/internal/ModManifest.h"

namespace pl::internal::mod {

void *acquireModHandle(const std::string &libraryPath);

std::optional<LoadedModEntry>
createLoadedModEntry(void *handle, JavaVM *vm, const RuntimeModInfo &modInfo,
                     std::string normalizedLibraryPath);

bool enableLoadedModEntry(const LoadedModEntry &entry);

bool disableLoadedModEntry(const LoadedModEntry &entry);

bool unloadLoadedModEntry(const LoadedModEntry &entry);

} // namespace pl::internal::mod
