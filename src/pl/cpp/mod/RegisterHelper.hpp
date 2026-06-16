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

template <Loadable T> bool loadAndEnable(T &myMod, NativeMod &self) {
  bindToMod(myMod, self);

  if (!self.runLoad()) {
    self.getLogger().error("Failed to load mod {}", self.getName());
    return false;
  }

  if (!self.runEnable()) {
    self.getLogger().error("Failed to enable mod {}", self.getName());
    return false;
  }

  return true;
}

} // namespace pl::mod

#define PL_REGISTER_MOD(CLAZZ, BINDER)                                         \
  extern "C" {                                                                 \
  PL_SHARED_EXPORT void LeviMod_Load(JavaVM *vm, const PLModInfo *mod_info) {  \
    static_assert(::pl::mod::Loadable<CLAZZ>, #CLAZZ " must be Loadable");     \
    auto &self = ::pl::mod::NativeMod::createCurrent(vm, mod_info);            \
    try {                                                                      \
      (void)::pl::mod::loadAndEnable((BINDER), self);                          \
    } catch (const std::exception &ex) {                                       \
      self.getLogger().error("Unhandled exception while loading mod {}: {}",   \
                             self.getName(), ex.what());                       \
    } catch (...) {                                                            \
      self.getLogger().error(                                                  \
          "Unhandled unknown exception while loading mod {}", self.getName()); \
    }                                                                          \
  }                                                                            \
  }
