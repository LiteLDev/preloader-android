#pragma once
#include "pl/api/Macro.h"
#include <cstdint>
#include <string>

#ifdef __cplusplus
namespace pl::signature {
#endif
PLCAPI uintptr_t pl_resolve_signature(const char *signature,
                                      const char *moduleName);
#ifdef __cplusplus
} // namespace pl::hook
#endif
