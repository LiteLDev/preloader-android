#pragma once
#include <cstdint>
#include <string>
#include "pl/internal/Macro.h"

namespace pl::signature {

uintptr_t resolveSignature(const std::string &signature,
                           const std::string &moduleName = "libminecraftpe.so");

} // namespace signature

#ifdef __cplusplus
extern "C" {
#endif

PLCAPI uintptr_t resolve_signature(const char* signature, const char* moduleName);

#ifdef __cplusplus
}
#endif