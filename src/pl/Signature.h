#pragma once
#include <cstdint>
#include <string>
#include "pl/internal/Macro.h"

#ifdef __cplusplus
namespace pl::signature {
#endif
    PLCAPI uintptr_t pl_resolve_signature(const char* signature, const char* moduleName);
#ifdef __cplusplus
} // namespace pl::hook
#endif


namespace pl::signature {
    [[deprecated("use pl_resolve_signature() instead")]]
    uintptr_t resolveSignature(const std::string &signature,
                               const std::string &moduleName = "libminecraftpe.so");

} // namespace signature