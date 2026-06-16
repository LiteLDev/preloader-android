#pragma once

#include <cstddef>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <utility>

#include "pl/Logger.h"
#include "pl/c/Mod.h"

namespace pl::mod {

class Mod {
public:
  enum class State {
    Unloaded,
    Loaded,
    Enabled,
  };

  using CallbackFn = std::function<bool(Mod &)>;

  Mod(JavaVM *vm, const PLModInfo *info)
      : javaVm(vm),
        modId(copyString(info, &PLModInfo::mod_id,
                         offsetof(PLModInfo, mod_id) + sizeof(const char *))),
        displayName(copyDisplayName(info)),
        author(copyString(info, &PLModInfo::author,
                          offsetof(PLModInfo, author) + sizeof(const char *))),
        version(
            copyString(info, &PLModInfo::version,
                       offsetof(PLModInfo, version) + sizeof(const char *))),
        entryPath(
            copyString(info, &PLModInfo::entry_path,
                       offsetof(PLModInfo, entry_path) + sizeof(const char *))),
        entryFileName(copyString(info, &PLModInfo::entry_file_name,
                                 offsetof(PLModInfo, entry_file_name) +
                                     sizeof(const char *))),
        libraryPath(copyString(info, &PLModInfo::library_path,
                               offsetof(PLModInfo, library_path) +
                                   sizeof(const char *))),
        iconPath(
            copyString(info, &PLModInfo::icon_path,
                       offsetof(PLModInfo, icon_path) + sizeof(const char *))),
        manifestPath(copyString(info, &PLModInfo::manifest_path,
                                offsetof(PLModInfo, manifest_path) +
                                    sizeof(const char *))),
        modRootPath(copyString(info, &PLModInfo::mod_root_path,
                               offsetof(PLModInfo, mod_root_path) +
                                   sizeof(const char *))),
        dataDir(modRootPath / "data"), configDir(modRootPath / "config"),
        resourceDir(modRootPath / "resources"),
        logger(&pl::log::Logger::getOrCreate(
            displayName.empty() ? fallbackName(modId) : displayName)) {}

  virtual ~Mod() = default;

  [[nodiscard]] State getState() const { return state; }
  [[nodiscard]] bool isEnabled() const { return state == State::Enabled; }
  [[nodiscard]] bool isLoaded() const { return state == State::Loaded; }
  [[nodiscard]] bool isUnloaded() const { return state == State::Unloaded; }
  [[nodiscard]] bool isDisabled() const { return state != State::Enabled; }

  [[nodiscard]] const std::string &getId() const { return modId; }
  [[nodiscard]] const std::string &getName() const { return displayName; }
  [[nodiscard]] const std::string &getAuthor() const { return author; }
  [[nodiscard]] const std::string &getVersion() const { return version; }
  [[nodiscard]] const std::string &getEntryPath() const { return entryPath; }
  [[nodiscard]] const std::string &getEntryFileName() const {
    return entryFileName;
  }
  [[nodiscard]] const std::string &getIconPath() const { return iconPath; }

  [[nodiscard]] const std::filesystem::path &getModDir() const {
    return modRootPath;
  }
  [[nodiscard]] const std::filesystem::path &getDataDir() const {
    return dataDir;
  }
  [[nodiscard]] const std::filesystem::path &getConfigDir() const {
    return configDir;
  }
  [[nodiscard]] const std::filesystem::path &getResourceDir() const {
    return resourceDir;
  }
  [[nodiscard]] const std::filesystem::path &getManifestPath() const {
    return manifestPath;
  }
  [[nodiscard]] const std::filesystem::path &getLibraryPath() const {
    return libraryPath;
  }
  [[nodiscard]] JavaVM *getJavaVM() const { return javaVm; }
  [[nodiscard]] pl::log::Logger &getLogger() const { return *logger; }

  void onLoad(CallbackFn callback) { loadCallback = std::move(callback); }
  void onEnable(CallbackFn callback) { enableCallback = std::move(callback); }
  void onDisable(CallbackFn callback) { disableCallback = std::move(callback); }
  void onUnload(CallbackFn callback) { unloadCallback = std::move(callback); }

  bool runLoad() {
    if (state != State::Unloaded)
      return true;

    if (!runCallback(loadCallback))
      return false;

    state = State::Loaded;
    return true;
  }

  bool runEnable() {
    if (state == State::Enabled)
      return true;

    if (state != State::Loaded)
      return false;

    if (!runCallback(enableCallback))
      return false;

    state = State::Enabled;
    return true;
  }

  bool runDisable() {
    if (state != State::Enabled)
      return true;

    if (!runCallback(disableCallback))
      return false;

    state = State::Loaded;
    return true;
  }

  bool runUnload() {
    if (state == State::Unloaded)
      return true;

    if (!runCallback(unloadCallback))
      return false;

    state = State::Unloaded;
    onLoad({});
    onEnable({});
    onDisable({});
    onUnload({});
    return true;
  }

private:
  JavaVM *javaVm{};
  std::string modId;
  std::string displayName;
  std::string author;
  std::string version;
  std::string entryPath;
  std::string entryFileName;
  std::filesystem::path libraryPath;
  std::string iconPath;
  std::filesystem::path manifestPath;
  std::filesystem::path modRootPath;
  std::filesystem::path dataDir;
  std::filesystem::path configDir;
  std::filesystem::path resourceDir;
  pl::log::Logger *logger{};
  State state{State::Unloaded};
  CallbackFn loadCallback;
  CallbackFn enableCallback;
  CallbackFn disableCallback;
  CallbackFn unloadCallback;

  using StringField = const char *PLModInfo::*;

  static std::string copyString(const PLModInfo *info, StringField field,
                                size_t requiredSize) {
    if (!info || info->size < requiredSize)
      return {};

    const char *value = info->*field;
    return value ? std::string(value) : std::string{};
  }

  static std::string fallbackName(const std::string &id) {
    return id.empty() ? "LeviMod" : id;
  }

  static std::string copyDisplayName(const PLModInfo *info) {
    std::string name =
        copyString(info, &PLModInfo::display_name,
                   offsetof(PLModInfo, display_name) + sizeof(const char *));
    if (!name.empty())
      return name;

    return fallbackName(
        copyString(info, &PLModInfo::mod_id,
                   offsetof(PLModInfo, mod_id) + sizeof(const char *)));
  }

  bool runCallback(const CallbackFn &callback) {
    if (!callback)
      return true;

    return callback(*this);
  }
};

} // namespace pl::mod
