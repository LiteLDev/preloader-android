#pragma once
#include <filesystem>
#include <jni.h>
#include <optional>
#include <string>

namespace ModManager {
[[gnu::visibility("hidden")]] bool
LoadModLibrary(const std::filesystem::path &libraryPath,
               const std::optional<std::filesystem::path> &sourceModDirectory,
               JavaVM *vm);
[[gnu::visibility("hidden")]] void EnableLoadedMods();
[[gnu::visibility("hidden")]] void DisableAndUnloadLoadedMods();
} // namespace ModManager
