#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "pl/c/Signature.h"

namespace pl::signature {

inline uintptr_t pl_resolve_signature(const char *signature,
                                      const char *moduleName) {
  return ::pl_resolve_signature(signature, moduleName);
}

PLAPI uintptr_t resolveSignature(const std::string &signature,
                                 const std::string &moduleName);

PLAPI std::unordered_map<std::string, uintptr_t>
resolveSignatures(const std::vector<std::string> &signatures,
                  const std::string &moduleName);

} // namespace pl::signature
