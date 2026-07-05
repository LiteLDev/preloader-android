#pragma once

#include <stdbool.h>

#include "pl/legacy/LegacyMacro.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void *PLFuncPtr;
typedef PLFuncPtr FuncPtr;

typedef enum PLHookPriority {
  PL_HOOK_PRIORITY_HIGHEST = 0,
  PL_HOOK_PRIORITY_HIGH = 100,
  PL_HOOK_PRIORITY_NORMAL = 200,
  PL_HOOK_PRIORITY_LOW = 300,
  PL_HOOK_PRIORITY_LOWEST = 400,
} PLHookPriority;

PL_LEGACY_EXPORT int pl_hook(PLFuncPtr target, PLFuncPtr detour,
                             PLFuncPtr *originalFunc,
                             PLHookPriority priority);

PL_LEGACY_EXPORT bool pl_unhook(PLFuncPtr target, PLFuncPtr detour);

#ifdef __cplusplus
} // extern "C"
#endif

