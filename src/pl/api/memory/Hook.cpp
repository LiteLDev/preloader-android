#include "pl/api/memory/Hook.h"
#include "pl/api/memory/Memory.h"
#include "pl/Hook.h"
#include "pl/Signature.h"

#include <mutex>
#include <set>
#include <type_traits>
#include <unordered_map>
#include <chrono>
#include <inttypes.h>

namespace memory {

FuncPtr resolveIdentifier(char const *identifier, char const *moduleName) {
  return reinterpret_cast<FuncPtr>(
      pl::signature::pl_resolve_signature(identifier, moduleName));
}

FuncPtr resolveIdentifier(std::initializer_list<const char *> identifiers,
                          char const *moduleName) {
  for (const auto &identifier : identifiers) {
    FuncPtr result = resolveIdentifier(identifier, moduleName);
    if (result != nullptr) {
      return result;
    }
  }
  return nullptr;
}

int hook(FuncPtr target, FuncPtr detour, FuncPtr *originalFunc,
         HookPriority priority, bool) {
  return pl::hook::pl_hook(target, detour, originalFunc,
                           pl::hook::Priority(priority));
}

bool unhook(FuncPtr target, FuncPtr detour, bool) {
  return pl::hook::pl_unhook(target, detour);
}

} // namespace memory