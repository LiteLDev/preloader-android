#include "NativeModLifecycle.h"

#include <dlfcn.h>

#include <exception>
#include <memory>
#include <utility>

#include "pl/Logger.hpp"
#include "pl/runtime/ModMenuBridge.h"

namespace pl::internal::mod {
namespace {
constexpr const char *kModLoadSymbol = "LeviMod_Load";
constexpr const char *kLifecycleLoadSymbol = "PLMod_Load";
constexpr const char *kLifecycleEnableSymbol = "PLMod_Enable";
constexpr const char *kLifecycleDisableSymbol = "PLMod_Disable";
constexpr const char *kLifecycleUnloadSymbol = "PLMod_Unload";
constexpr const char *kCppRegistrationSymbol = pl::mod::ModRegistrationSymbol;
constexpr int kLoadedModLookupFlags = RTLD_NOLOAD | RTLD_NOW;
constexpr int kModDlopenFlags = RTLD_NOW | RTLD_GLOBAL;

using PLModLifecycleLoadFunc = bool (*)(JavaVM *vm, const PLModInfo *modInfo);
using PLCppModRegistrationFunc = pl::mod::ModRegistration *(*)();

PLModLoadFunc resolveLegacyModEntry(void *handle) {
  return reinterpret_cast<PLModLoadFunc>(dlsym(handle, kModLoadSymbol));
}

PLModLifecycleLoadFunc resolveLifecycleLoad(void *handle) {
  return reinterpret_cast<PLModLifecycleLoadFunc>(
      dlsym(handle, kLifecycleLoadSymbol));
}

PLModLifecycleFunc resolveLifecycleFunc(void *handle, const char *symbol) {
  return reinterpret_cast<PLModLifecycleFunc>(dlsym(handle, symbol));
}

pl::mod::ModRegistration *resolveCppModRegistration(void *handle) {
  if (auto resolve = reinterpret_cast<PLCppModRegistrationFunc>(
          dlsym(handle, kCppRegistrationSymbol))) {
    return resolve();
  }

  return nullptr;
}

bool runCppLifecycle(pl::mod::NativeMod &nativeMod,
                     pl::mod::ModRegistration &registration,
                     pl::mod::ModContext &context,
                     pl::mod::LifecycleFunction lifecycle, const char *phase) {
  if (!lifecycle) {
    return true;
  }

  try {
    pl::mod::detail::ScopedCurrentMod current(&nativeMod);
    return lifecycle(registration.instance, context);
  } catch (const std::exception &ex) {
    preloaderLogger.error("Unhandled exception while {} mod {}: {}", phase,
                          context.name(), ex.what());
  } catch (...) {
    preloaderLogger.error("Unhandled unknown exception while {} mod {}", phase,
                          context.name());
  }
  return false;
}

bool hasCppRuntimeState(const LoadedModEntry &entry) {
  if (entry.cppRegistration && entry.cppNativeMod && entry.cppContext) {
    return true;
  }

  preloaderLogger.error("C++ lifecycle mod {} is missing registration state",
                        entry.modId);
  return false;
}

} // namespace

void *acquireModHandle(const std::string &libraryPath) {
  if (void *handle = dlopen(libraryPath.c_str(), kLoadedModLookupFlags)) {
    return handle;
  }

  return dlopen(libraryPath.c_str(), kModDlopenFlags);
}

std::optional<LoadedModEntry>
createLoadedModEntry(void *handle, JavaVM *vm, const RuntimeModInfo &modInfo,
                     std::string normalizedLibraryPath) {
  LoadedModEntry entry{
      .libraryPath = std::move(normalizedLibraryPath),
      .modId = modInfo.modId,
      .kind = LoadedModKind::NoEntry,
      .state = LoadedModState::Loaded,
  };

  auto cppModInfo = toCppModInfo(modInfo);
  auto cppNativeMod = std::make_shared<pl::mod::NativeMod>(vm, cppModInfo);
  pl::mod::ModRegistration *cppRegistration{};
  {
    pl::mod::detail::ScopedCurrentMod current(cppNativeMod.get());
    cppRegistration = resolveCppModRegistration(handle);
  }

  if (cppRegistration) {
    entry.kind = LoadedModKind::CppLifecycle;
    entry.cppRegistration = cppRegistration;
    entry.cppNativeMod = std::move(cppNativeMod);
    entry.cppContext =
        std::make_shared<pl::mod::ModContext>(vm, std::move(cppModInfo));

    bool loaded = false;
    {
      pl::runtime::ScopedModMenuOwner owner(modInfo.modId);
      loaded = runCppLifecycle(*entry.cppNativeMod, *entry.cppRegistration,
                               *entry.cppContext, entry.cppRegistration->load,
                               "loading");
    }
    if (!loaded) {
      preloaderLogger.error("Failed to run C++ mod registration for {}",
                            modInfo.modId);
      return std::nullopt;
    }
    entry.cppNativeMod->setState(pl::mod::NativeMod::State::Loaded);
    return entry;
  }

  if (auto load = resolveLifecycleLoad(handle)) {
    entry.kind = LoadedModKind::Lifecycle;
    entry.enable = resolveLifecycleFunc(handle, kLifecycleEnableSymbol);
    entry.disable = resolveLifecycleFunc(handle, kLifecycleDisableSymbol);
    entry.unload = resolveLifecycleFunc(handle, kLifecycleUnloadSymbol);

    bool loaded = false;
    {
      pl::runtime::ScopedModMenuOwner owner(modInfo.modId);
      loaded = load(vm, &modInfo.legacyInfo);
    }
    if (!loaded) {
      preloaderLogger.error("Failed to run PLMod_Load for {}", modInfo.modId);
      return std::nullopt;
    }
    return entry;
  }

  if (auto legacyLoad = resolveLegacyModEntry(handle)) {
    entry.kind = LoadedModKind::Legacy;
    legacyLoad(vm, &modInfo.legacyInfo);
    return entry;
  }

  preloaderLogger.warn("Mod library {} does not export a lifecycle entry",
                       entry.libraryPath);
  return entry;
}

bool enableLoadedModEntry(const LoadedModEntry &entry) {
  if (entry.kind == LoadedModKind::CppLifecycle) {
    if (!hasCppRuntimeState(entry)) {
      return false;
    }

    bool enabled = false;
    {
      pl::runtime::ScopedModMenuOwner owner(entry.modId);
      enabled = runCppLifecycle(*entry.cppNativeMod, *entry.cppRegistration,
                                *entry.cppContext,
                                entry.cppRegistration->enable, "enabling");
    }
    if (!enabled) {
      preloaderLogger.error("Failed to enable C++ lifecycle mod {}",
                            entry.modId);
      return false;
    }

    entry.cppNativeMod->setState(pl::mod::NativeMod::State::Enabled);
    return true;
  }

  if (entry.kind != LoadedModKind::Lifecycle) {
    return false;
  }

  if (!entry.enable) {
    preloaderLogger.warn("Lifecycle mod {} does not export {}", entry.modId,
                         kLifecycleEnableSymbol);
    return false;
  }

  bool enabled = false;
  {
    pl::runtime::ScopedModMenuOwner owner(entry.modId);
    enabled = entry.enable();
  }
  if (!enabled) {
    preloaderLogger.error("Failed to enable lifecycle mod {}", entry.modId);
    return false;
  }

  return true;
}

bool disableLoadedModEntry(const LoadedModEntry &entry) {
  bool disabled = true;
  if (entry.kind == LoadedModKind::CppLifecycle) {
    if (!hasCppRuntimeState(entry)) {
      return false;
    }

    pl::runtime::ScopedModMenuOwner owner(entry.modId);
    disabled = runCppLifecycle(*entry.cppNativeMod, *entry.cppRegistration,
                               *entry.cppContext,
                               entry.cppRegistration->disable, "disabling");
  } else if (entry.kind == LoadedModKind::Lifecycle && entry.disable) {
    pl::runtime::ScopedModMenuOwner owner(entry.modId);
    disabled = entry.disable();
    if (!disabled) {
      preloaderLogger.error("Failed to disable lifecycle mod {}", entry.modId);
    }
  } else if (entry.kind == LoadedModKind::Lifecycle) {
    preloaderLogger.warn("Lifecycle mod {} does not export {}", entry.modId,
                         kLifecycleDisableSymbol);
  } else {
    return false;
  }

  if (disabled && entry.cppNativeMod) {
    entry.cppNativeMod->setState(pl::mod::NativeMod::State::Loaded);
  }
  return disabled;
}

bool unloadLoadedModEntry(const LoadedModEntry &entry) {
  bool unloaded = true;
  if (entry.kind == LoadedModKind::CppLifecycle) {
    if (!hasCppRuntimeState(entry)) {
      return false;
    }

    pl::runtime::ScopedModMenuOwner owner(entry.modId);
    unloaded = runCppLifecycle(*entry.cppNativeMod, *entry.cppRegistration,
                               *entry.cppContext,
                               entry.cppRegistration->unload, "unloading");
    if (!unloaded) {
      preloaderLogger.error("Failed to unload C++ lifecycle mod {}",
                            entry.modId);
    }
  } else if (entry.kind == LoadedModKind::Lifecycle && entry.unload) {
    pl::runtime::ScopedModMenuOwner owner(entry.modId);
    unloaded = entry.unload();
    if (!unloaded) {
      preloaderLogger.error("Failed to unload lifecycle mod {}", entry.modId);
    }
  } else if (entry.kind == LoadedModKind::Lifecycle) {
    preloaderLogger.warn("Lifecycle mod {} does not export {}", entry.modId,
                         kLifecycleUnloadSymbol);
  } else {
    return false;
  }

  if (!unloaded) {
    return false;
  }

  if (entry.cppNativeMod) {
    entry.cppNativeMod->setState(pl::mod::NativeMod::State::Unloaded);
  }
  pl::runtime::UnregisterModulesForModId(entry.modId);
  return true;
}

} // namespace pl::internal::mod
