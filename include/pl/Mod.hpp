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

inline constexpr const char *ModRegistrationSymbol =
    "PLGetModRegistration";

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
concept Loadable = requires(T t, ModContext &context) {
  { t.load(context) } -> std::same_as<bool>;
};

template <typename T>
concept Enableable = requires(T t, ModContext &context) {
  { t.enable(context) } -> std::same_as<bool>;
};

template <typename T>
concept Disableable = requires(T t, ModContext &context) {
  { t.disable(context) } -> std::same_as<bool>;
};

template <typename T>
concept Unloadable = requires(T t, ModContext &context) {
  { t.unload(context) } -> std::same_as<bool>;
};

template <typename T> bool load(void *instance, ModContext &context) {
  return static_cast<T *>(instance)->load(context);
}

template <typename T> bool enable(void *instance, ModContext &context) {
  if constexpr (Enableable<T>) {
    return static_cast<T *>(instance)->enable(context);
  } else {
    return true;
  }
}

template <typename T> bool disable(void *instance, ModContext &context) {
  if constexpr (Disableable<T>) {
    return static_cast<T *>(instance)->disable(context);
  } else {
    return true;
  }
}

template <typename T> bool unload(void *instance, ModContext &context) {
  if constexpr (Unloadable<T>) {
    return static_cast<T *>(instance)->unload(context);
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
