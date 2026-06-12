#pragma once

#include <stdint.h>

#include "pl/c/Macro.h"

#ifdef __cplusplus
extern "C" {
#endif

PLAPI uintptr_t pl_resolve_signature(const char *signature,
                                     const char *moduleName);

#ifdef __cplusplus
} // extern "C"
#endif
