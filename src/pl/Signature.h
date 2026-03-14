#pragma once
#include <string>
#include "pl/api/Macro.h"
#include "pl/Logger.h"
#include "pl/api/Types.h"

#ifdef __cplusplus
namespace pl::signature {
#endif
    PLCAPI uintptr_t pl_resolve_signature(const char* signature, const char* moduleName);
    PLCAPI void* resolveMinecraftSignature(const char* sig, const char* name);
#ifdef __cplusplus
} // namespace pl::hook
#endif


namespace pl::signature {
    [[deprecated("use pl_resolve_signature() instead")]]
    uintptr_t resolveSignature(const std::string &signature,
                               const std::string &moduleName = "libminecraftpe.so");

} // namespace signature