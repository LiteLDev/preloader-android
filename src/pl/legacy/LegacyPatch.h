#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "pl/legacy/LegacyMacro.h"

#ifdef __cplusplus
extern "C" {
#endif

PL_LEGACY_EXPORT bool pl_patch_write_bytes(uintptr_t addr,
                                           const uint8_t *bytes, size_t len,
                                           const char *name);

PL_LEGACY_EXPORT bool pl_patch_write_hex(uintptr_t addr, const char *bytes,
                                         const char *name);

PL_LEGACY_EXPORT size_t pl_patch_read_bytes(uintptr_t addr, uint8_t *out,
                                            size_t len);

PL_LEGACY_EXPORT bool pl_patch_revert(const char *name);

PL_LEGACY_EXPORT void pl_patch_revert_all(void);

#ifdef __cplusplus
} // extern "C"
#endif

