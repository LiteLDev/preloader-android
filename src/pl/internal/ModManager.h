#pragma once
#include <filesystem>
#include <jni.h>
#include <optional>
#include <string>

#include "pl/c/Mod.h"

namespace ModManager {
[[gnu::visibility("hidden")]] bool
LoadModLibrary(const std::filesystem::path &libraryPath,
               const std::optional<std::filesystem::path> &sourceModDirectory,
               JavaVM *vm);
} // namespace ModManager
