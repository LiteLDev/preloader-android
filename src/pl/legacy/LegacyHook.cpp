#include "pl/legacy/LegacyHook.h"

#include "pl/memory/Hook.hpp"

extern "C" {

PL_LEGACY_EXPORT int pl_hook(PLFuncPtr target, PLFuncPtr detour,
                             PLFuncPtr *originalFunc,
                             PLHookPriority priority) {
  return pl::memory::hook(target, detour, originalFunc,
                          static_cast<pl::memory::HookPriority>(priority));
}

PL_LEGACY_EXPORT bool pl_unhook(PLFuncPtr target, PLFuncPtr detour) {
  return pl::memory::unhook(target, detour);
}

} // extern "C"

