#pragma once

#include "pl/c/Hook.h"

namespace pl::hook {

using FuncPtr = PLFuncPtr;

enum Priority : int {
  PriorityHighest = PL_HOOK_PRIORITY_HIGHEST,
  PriorityHigh = PL_HOOK_PRIORITY_HIGH,
  PriorityNormal = PL_HOOK_PRIORITY_NORMAL,
  PriorityLow = PL_HOOK_PRIORITY_LOW,
  PriorityLowest = PL_HOOK_PRIORITY_LOWEST,
};

inline PLHookPriority toC(Priority priority) {
  return static_cast<PLHookPriority>(static_cast<int>(priority));
}

inline int pl_hook(FuncPtr target, FuncPtr detour, FuncPtr *originalFunc,
                   Priority priority) {
  return ::pl_hook(target, detour, originalFunc, toC(priority));
}

inline bool pl_unhook(FuncPtr target, FuncPtr detour) {
  return ::pl_unhook(target, detour);
}

inline int hook(FuncPtr target, FuncPtr detour, FuncPtr *originalFunc,
                Priority priority = PriorityNormal) {
  return pl_hook(target, detour, originalFunc,
                 priority);
}

inline bool unhook(FuncPtr target, FuncPtr detour) {
  return pl_unhook(target, detour);
}

} // namespace pl::hook
