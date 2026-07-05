#include "pl/legacy/LegacySignature.h"

#include "pl/memory/Signature.hpp"

extern "C" {

PL_LEGACY_EXPORT uintptr_t pl_resolve_signature(const char *signature,
                                                const char *moduleName) {
  if (!signature || !moduleName) {
    return 0;
  }
  return pl::memory::resolveSignature(signature, moduleName);
}

} // extern "C"
