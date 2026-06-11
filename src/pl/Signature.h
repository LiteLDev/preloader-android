#pragma once
#include "pl/api/Macro.h"
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#ifdef __cplusplus
namespace pl::signature {
#endif
PLCAPI uintptr_t pl_resolve_signature(const char *signature,
                                      const char *moduleName);
#ifdef __cplusplus
PLAPI uintptr_t resolveSignature(const std::string &signature,
                                 const std::string &moduleName);
PLAPI std::unordered_map<std::string, uintptr_t>
resolveSignatures(const std::vector<std::string> &signatures,
                  const std::string &moduleName);
} // namespace pl::signature
#endif
