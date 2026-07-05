#pragma once

#include <filesystem>
#include <optional>
#include <string>

#include "pl/Mod.hpp"
#include "pl/legacy/LegacyMod.h"

namespace pl::internal::mod {

struct RuntimeModInfo {
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
  PLModInfo legacyInfo{};
};

std::string normalizeLibraryPath(const std::filesystem::path &libraryPath);

bool createRuntimeModInfo(
    const std::filesystem::path &libraryPath,
    const std::optional<std::filesystem::path> &sourceModDirectory,
    RuntimeModInfo &storage);

pl::mod::ModInfo toCppModInfo(const RuntimeModInfo &storage);

} // namespace pl::internal::mod
