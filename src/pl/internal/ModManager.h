#pragma once
#include <filesystem>
#include <jni.h>
#include <string>

#include "pl/Mod.h"

namespace ModManager {
[[gnu::visibility("hidden")]] bool LoadModLibrary(
    const std::filesystem::path &libraryPath,
    JavaVM *vm);
} // namespace ModManager
