#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "pl/Mod.hpp"

namespace pl::internal::mod {

using PLModLifecycleFunc = bool (*)();

enum class LoadedModKind {
  Legacy,
  Lifecycle,
  CppLifecycle,
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
  pl::mod::ModRegistration *cppRegistration{};
  std::shared_ptr<pl::mod::NativeMod> cppNativeMod;
  std::shared_ptr<pl::mod::ModContext> cppContext;
};

bool isModAlreadyRegistered(const std::string &normalizedLibraryPath);

void registerLoadedMod(const std::string &normalizedLibraryPath,
                       LoadedModEntry entry);

std::vector<std::string> getLoadedModKeysSnapshot();

std::optional<LoadedModEntry> getLoadedModEntry(const std::string &key);

void setLoadedModState(const std::string &key, LoadedModState state);

} // namespace pl::internal::mod
