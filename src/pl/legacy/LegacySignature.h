#pragma once

#include <stdint.h>

#include "pl/legacy/LegacyMacro.h"

#ifdef __cplusplus
extern "C" {
#endif

PL_LEGACY_EXPORT uintptr_t pl_resolve_signature(const char *signature,
                                                const char *moduleName);

#ifdef __cplusplus
} // extern "C"
#endif
