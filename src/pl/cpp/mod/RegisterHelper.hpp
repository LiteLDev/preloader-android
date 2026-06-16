#pragma once

#include <concepts>
#include <exception>

#include "pl/c/Macro.h"
#include "pl/cpp/mod/NativeMod.hpp"

namespace pl::mod {

template <typename T>
concept Loadable = requires(T t) {
  { t.load() } -> std::same_as<bool>;
};

template <typename T>
concept Enableable = requires(T t) {
  { t.enable() } -> std::same_as<bool>;
};

template <typename T>
concept Disableable = requires(T t) {
  { t.disable() } -> std::same_as<bool>;
};

template <typename T>
concept Unloadable = requires(T t) {
  { t.unload() } -> std::same_as<bool>;
};

template <Loadable T> void bindToMod(T &myMod, Mod &self) {
  self.onLoad([&myMod](Mod &) { return myMod.load(); });

  if constexpr (Enableable<T>) {
    self.onEnable([&myMod](Mod &) { return myMod.enable(); });
  }

  if constexpr (Disableable<T>) {
    self.onDisable([&myMod](Mod &) { return myMod.disable(); });
  }

  if constexpr (Unloadable<T>) {
    self.onUnload([&myMod](Mod &) { return myMod.unload(); });
  }
}

template <Loadable T> bool load(T &myMod, NativeMod &self) {
  bindToMod(myMod, self);

  if (!self.runLoad()) {
    self.getLogger().error("Failed to load mod {}", self.getName());
    return false;
  }

  return true;
}

inline bool enable(NativeMod &self) {
  if (!self.runEnable()) {
    self.getLogger().error("Failed to enable mod {}", self.getName());
    return false;
  }

  return true;
}

inline bool disable(NativeMod &self) {
  if (!self.runDisable()) {
    self.getLogger().error("Failed to disable mod {}", self.getName());
    return false;
  }

  return true;
}

inline bool unload(NativeMod &self) {
  if (!self.runUnload()) {
    self.getLogger().error("Failed to unload mod {}", self.getName());
    return false;
  }

  NativeMod::clearCurrent();
  return true;
}

} // namespace pl::mod

#define PL_REGISTER_MOD(CLAZZ, BINDER)                                         \
  extern "C" {                                                                 \
  PL_SHARED_EXPORT bool PLMod_Load(JavaVM *vm, const PLModInfo *mod_info) {    \
    static_assert(::pl::mod::Loadable<CLAZZ>, #CLAZZ " must be Loadable");     \
    auto &self = ::pl::mod::NativeMod::createCurrent(vm, mod_info);            \
    try {                                                                      \
      const bool loaded = ::pl::mod::load((BINDER), self);                     \
      if (!loaded)                                                             \
        ::pl::mod::NativeMod::clearCurrent();                                  \
      return loaded;                                                           \
    } catch (const std::exception &ex) {                                       \
      self.getLogger().error("Unhandled exception while loading mod {}: {}",   \
                             self.getName(), ex.what());                       \
    } catch (...) {                                                            \
      self.getLogger().error(                                                  \
          "Unhandled unknown exception while loading mod {}", self.getName()); \
    }                                                                          \
    ::pl::mod::NativeMod::clearCurrent();                                      \
    return false;                                                              \
  }                                                                            \
  PL_SHARED_EXPORT bool PLMod_Enable() {                                       \
    auto self = ::pl::mod::NativeMod::current();                               \
    if (!self)                                                                 \
      return false;                                                            \
    try {                                                                      \
      return ::pl::mod::enable(*self);                                         \
    } catch (const std::exception &ex) {                                       \
      self->getLogger().error("Unhandled exception while enabling mod {}: {}", \
                              self->getName(), ex.what());                     \
    } catch (...) {                                                            \
      self->getLogger().error(                                                 \
          "Unhandled unknown exception while enabling mod {}",                 \
          self->getName());                                                    \
    }                                                                          \
    return false;                                                              \
  }                                                                            \
  PL_SHARED_EXPORT bool PLMod_Disable() {                                      \
    auto self = ::pl::mod::NativeMod::current();                               \
    if (!self)                                                                 \
      return true;                                                             \
    try {                                                                      \
      return ::pl::mod::disable(*self);                                        \
    } catch (const std::exception &ex) {                                       \
      self->getLogger().error("Unhandled exception while disabling mod {}: {}",\
                              self->getName(), ex.what());                     \
    } catch (...) {                                                            \
      self->getLogger().error(                                                 \
          "Unhandled unknown exception while disabling mod {}",                \
          self->getName());                                                    \
    }                                                                          \
    return false;                                                              \
  }                                                                            \
  PL_SHARED_EXPORT bool PLMod_Unload() {                                       \
    auto self = ::pl::mod::NativeMod::current();                               \
    if (!self)                                                                 \
      return true;                                                             \
    try {                                                                      \
      return ::pl::mod::unload(*self);                                         \
    } catch (const std::exception &ex) {                                       \
      self->getLogger().error("Unhandled exception while unloading mod {}: {}",\
                              self->getName(), ex.what());                     \
    } catch (...) {                                                            \
      self->getLogger().error(                                                 \
          "Unhandled unknown exception while unloading mod {}",                \
          self->getName());                                                    \
    }                                                                          \
    return false;                                                              \
  }                                                                            \
  }
