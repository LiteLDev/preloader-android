#pragma once

/**
 * @file Mod.hpp
 * @brief Mod lifecycle registration API.
 */

#include <concepts>
#include <exception>
#include <filesystem>
#include <jni.h>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>

#include "pl/Export.hpp"
#include "pl/Logger.hpp"

namespace pl::mod {

inline constexpr const char *ModRegistrationSymbol = "PLGetModRegistration";

/**
 * @brief Manifest and filesystem metadata resolved by the preloader.
 */
struct ModInfo {
  std::string id;
  std::string displayName;
  std::string author;
  std::string version;
  std::filesystem::path entryPath;
  std::string entryFileName;
  std::filesystem::path libraryPath;
  std::filesystem::path iconPath;
  std::filesystem::path manifestPath;
  std::filesystem::path modRootPath;
};

/**
 * @brief Runtime context passed to each C++ mod lifecycle phase.
 */
class ModContext {
public:
  ModContext(JavaVM *javaVm, ModInfo info)
      : mJavaVm(javaVm), mInfo(std::move(info)),
        mLogger(&pl::log::Logger::getOrCreate(mInfo.displayName.empty()
                                                  ? fallbackName(mInfo.id)
                                                  : mInfo.displayName)) {}

  [[nodiscard]] JavaVM *javaVm() const noexcept { return mJavaVm; }
  [[nodiscard]] const ModInfo &info() const noexcept { return mInfo; }
  [[nodiscard]] pl::log::Logger &logger() const noexcept { return *mLogger; }

  [[nodiscard]] const std::string &id() const noexcept { return mInfo.id; }
  [[nodiscard]] const std::string &name() const noexcept {
    return mInfo.displayName;
  }
  [[nodiscard]] const std::filesystem::path &modRootPath() const noexcept {
    return mInfo.modRootPath;
  }
  [[nodiscard]] std::filesystem::path dataDir() const {
    return mInfo.modRootPath / "data";
  }
  [[nodiscard]] std::filesystem::path configDir() const {
    return mInfo.modRootPath / "config";
  }
  [[nodiscard]] std::filesystem::path resourceDir() const {
    return mInfo.modRootPath / "resources";
  }

private:
  JavaVM *mJavaVm{};
  ModInfo mInfo;
  pl::log::Logger *mLogger{};

  static std::string fallbackName(const std::string &id) {
    return id.empty() ? "LeviMod" : id;
  }
};

/**
 * @brief Runtime mod object for the currently executing native mod.
 */
class NativeMod {
public:
  enum class State {
    Unloaded,
    Loaded,
    Enabled,
  };

  NativeMod(JavaVM *javaVm, ModInfo info)
      : mJavaVm(javaVm), mInfo(std::move(info)),
        mDataDir(mInfo.modRootPath / "data"),
        mConfigDir(mInfo.modRootPath / "config"),
        mResourceDir(mInfo.modRootPath / "resources"),
        mLogger(&pl::log::Logger::getOrCreate(mInfo.displayName.empty()
                                                  ? fallbackName(mInfo.id)
                                                  : mInfo.displayName)) {}

  [[nodiscard]] State getState() const noexcept { return mState; }
  [[nodiscard]] bool isEnabled() const noexcept {
    return mState == State::Enabled;
  }
  [[nodiscard]] bool isLoaded() const noexcept {
    return mState == State::Loaded;
  }
  [[nodiscard]] bool isUnloaded() const noexcept {
    return mState == State::Unloaded;
  }
  [[nodiscard]] bool isDisabled() const noexcept {
    return mState != State::Enabled;
  }

  [[nodiscard]] const std::string &getId() const noexcept { return mInfo.id; }
  [[nodiscard]] const std::string &getName() const noexcept {
    return mInfo.displayName;
  }
  [[nodiscard]] const std::string &getAuthor() const noexcept {
    return mInfo.author;
  }
  [[nodiscard]] const std::string &getVersion() const noexcept {
    return mInfo.version;
  }
  [[nodiscard]] const std::filesystem::path &getEntryPath() const noexcept {
    return mInfo.entryPath;
  }
  [[nodiscard]] const std::string &getEntryFileName() const noexcept {
    return mInfo.entryFileName;
  }
  [[nodiscard]] const std::filesystem::path &getIconPath() const noexcept {
    return mInfo.iconPath;
  }
  [[nodiscard]] const std::filesystem::path &getModDir() const noexcept {
    return mInfo.modRootPath;
  }
  [[nodiscard]] const std::filesystem::path &getDataDir() const noexcept {
    return mDataDir;
  }
  [[nodiscard]] const std::filesystem::path &getConfigDir() const noexcept {
    return mConfigDir;
  }
  [[nodiscard]] const std::filesystem::path &getResourceDir() const noexcept {
    return mResourceDir;
  }
  [[nodiscard]] const std::filesystem::path &getManifestPath() const noexcept {
    return mInfo.manifestPath;
  }
  [[nodiscard]] const std::filesystem::path &getLibraryPath() const noexcept {
    return mInfo.libraryPath;
  }
  [[nodiscard]] JavaVM *getJavaVM() const noexcept { return mJavaVm; }
  [[nodiscard]] pl::log::Logger &getLogger() const noexcept { return *mLogger; }

  /**
   * @brief Returns the mod currently being registered or executing lifecycle.
   */
  [[nodiscard]] PL_EXPORT static NativeMod *current() noexcept;

  void setState(State state) noexcept { mState = state; }

private:
  JavaVM *mJavaVm{};
  ModInfo mInfo;
  std::filesystem::path mDataDir;
  std::filesystem::path mConfigDir;
  std::filesystem::path mResourceDir;
  pl::log::Logger *mLogger{};
  State mState{State::Unloaded};

  [[nodiscard]] static std::string fallbackName(const std::string &id) {
    return id.empty() ? "LeviMod" : id;
  }
};

using LifecycleFunction = bool (*)(void *instance, ModContext &context);

/**
 * @brief Type-erased lifecycle dispatch table exported by a C++ mod.
 */
struct ModRegistration {
  void *instance{};
  LifecycleFunction load{};
  LifecycleFunction enable{};
  LifecycleFunction disable{};
  LifecycleFunction unload{};
};

namespace detail {

template <typename T>
concept LoadableWithContext = requires(T t, ModContext &context) {
  { t.load(context) } -> std::same_as<bool>;
};

template <typename T>
concept LoadableWithoutContext = requires(T t) {
  { t.load() } -> std::same_as<bool>;
};

template <typename T>
concept Loadable = LoadableWithContext<T> || LoadableWithoutContext<T>;

template <typename T>
concept EnableableWithContext = requires(T t, ModContext &context) {
  { t.enable(context) } -> std::same_as<bool>;
};

template <typename T>
concept EnableableWithoutContext = requires(T t) {
  { t.enable() } -> std::same_as<bool>;
};

template <typename T>
concept DisableableWithContext = requires(T t, ModContext &context) {
  { t.disable(context) } -> std::same_as<bool>;
};

template <typename T>
concept DisableableWithoutContext = requires(T t) {
  { t.disable() } -> std::same_as<bool>;
};

template <typename T>
concept UnloadableWithContext = requires(T t, ModContext &context) {
  { t.unload(context) } -> std::same_as<bool>;
};

template <typename T>
concept UnloadableWithoutContext = requires(T t) {
  { t.unload() } -> std::same_as<bool>;
};

class ScopedCurrentMod {
public:
  explicit ScopedCurrentMod(NativeMod *current) noexcept;
  ScopedCurrentMod(const ScopedCurrentMod &) = delete;
  ScopedCurrentMod &operator=(const ScopedCurrentMod &) = delete;
  ~ScopedCurrentMod();

private:
  NativeMod *mPrevious{};
};

template <typename T> bool load(void *instance, ModContext &context) {
  if constexpr (LoadableWithContext<T>) {
    return static_cast<T *>(instance)->load(context);
  } else {
    return static_cast<T *>(instance)->load();
  }
}

template <typename T> bool enable(void *instance, ModContext &context) {
  if constexpr (EnableableWithContext<T>) {
    return static_cast<T *>(instance)->enable(context);
  } else if constexpr (EnableableWithoutContext<T>) {
    return static_cast<T *>(instance)->enable();
  } else {
    return true;
  }
}

template <typename T> bool disable(void *instance, ModContext &context) {
  if constexpr (DisableableWithContext<T>) {
    return static_cast<T *>(instance)->disable(context);
  } else if constexpr (DisableableWithoutContext<T>) {
    return static_cast<T *>(instance)->disable();
  } else {
    return true;
  }
}

template <typename T> bool unload(void *instance, ModContext &context) {
  if constexpr (UnloadableWithContext<T>) {
    return static_cast<T *>(instance)->unload(context);
  } else if constexpr (UnloadableWithoutContext<T>) {
    return static_cast<T *>(instance)->unload();
  } else {
    return true;
  }
}

template <Loadable T> ModRegistration makeRegistration(T &instance) {
  return ModRegistration{
      .instance = std::addressof(instance),
      .load = load<T>,
      .enable = enable<T>,
      .disable = disable<T>,
      .unload = unload<T>,
  };
}

template <typename T> T &materializeModInstance(T &instance) {
  return instance;
}

template <typename T> T &materializeModInstance(T &&instance) {
  static T owned(std::move(instance));
  return owned;
}

} // namespace detail

} // namespace pl::mod

namespace ll::mod {
using ModContext = ::pl::mod::ModContext;
using ModInfo = ::pl::mod::ModInfo;
using ModRegistration = ::pl::mod::ModRegistration;
using NativeMod = ::pl::mod::NativeMod;
} // namespace ll::mod

extern "C" {

/**
 * @brief Returns the C++ lifecycle registration for a loaded mod library.
 */
PL_EXPORT ::pl::mod::ModRegistration *PLGetModRegistration();

} // extern "C"

/**
 * @brief Registers a mod instance with the preloader.
 */
#define PL_REGISTER_MOD(TYPE, INSTANCE_EXPR)                                   \
  namespace {                                                                  \
  TYPE &plRegisteredModInstance() {                                            \
    static TYPE &instance =                                                    \
        ::pl::mod::detail::materializeModInstance<TYPE>((INSTANCE_EXPR));      \
    return instance;                                                           \
  }                                                                            \
  }                                                                            \
  extern "C" PL_EXPORT ::pl::mod::ModRegistration *PLGetModRegistration() {    \
    static auto registration =                                                 \
        ::pl::mod::detail::makeRegistration(plRegisteredModInstance());        \
    return &registration;                                                      \
  }
