#pragma once
#include <filesystem>
#include <jni.h>
#include <string>

#include "pl/Mod.h"

namespace ModManager {
bool IsModEnabled(const std::filesystem::path &configPath,
                  const std::string &modId);
[[gnu::visibility("hidden")]] bool LoadModLibrary(
    const std::filesystem::path &libraryPath,
    JavaVM *vm);
void LoadAndInitializeEnabledMods(const std::string &modsDir,
                                  const std::string &dataDir, JavaVM *vm);
} // namespace ModManager
