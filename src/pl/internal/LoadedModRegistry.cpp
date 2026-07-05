#include "LoadedModRegistry.h"

#include <mutex>
#include <unordered_map>
#include <utility>

namespace pl::internal::mod {
namespace {
std::mutex gLoadedModsMutex;
std::unordered_map<std::string, LoadedModEntry> gLoadedModLibraries;
std::vector<std::string> gLoadedModOrder;
} // namespace

bool isModAlreadyRegistered(const std::string &normalizedLibraryPath) {
  std::lock_guard<std::mutex> lock(gLoadedModsMutex);
  const auto it = gLoadedModLibraries.find(normalizedLibraryPath);
  return it != gLoadedModLibraries.end() &&
         it->second.state != LoadedModState::Unloaded;
}

void registerLoadedMod(const std::string &normalizedLibraryPath,
                       LoadedModEntry entry) {
  std::lock_guard<std::mutex> lock(gLoadedModsMutex);
  const auto [it, inserted] =
      gLoadedModLibraries.emplace(normalizedLibraryPath, LoadedModEntry{});
  it->second = std::move(entry);
  if (inserted) {
    gLoadedModOrder.push_back(normalizedLibraryPath);
  }
}

std::vector<std::string> getLoadedModKeysSnapshot() {
  std::lock_guard<std::mutex> lock(gLoadedModsMutex);
  std::vector<std::string> keys;
  keys.reserve(gLoadedModOrder.size());
  for (const auto &key : gLoadedModOrder) {
    const auto it = gLoadedModLibraries.find(key);
    if (it == gLoadedModLibraries.end()) {
      continue;
    }

    const auto &entry = it->second;
    if ((entry.kind == LoadedModKind::Lifecycle ||
         entry.kind == LoadedModKind::CppLifecycle) &&
        entry.state != LoadedModState::Unloaded) {
      keys.push_back(key);
    }
  }
  return keys;
}

std::optional<LoadedModEntry> getLoadedModEntry(const std::string &key) {
  std::lock_guard<std::mutex> lock(gLoadedModsMutex);
  const auto it = gLoadedModLibraries.find(key);
  if (it == gLoadedModLibraries.end()) {
    return std::nullopt;
  }

  return it->second;
}

void setLoadedModState(const std::string &key, LoadedModState state) {
  std::lock_guard<std::mutex> lock(gLoadedModsMutex);
  const auto it = gLoadedModLibraries.find(key);
  if (it != gLoadedModLibraries.end()) {
    it->second.state = state;
  }
}

} // namespace pl::internal::mod
