#pragma once
#include "pl/api/Macro.h"
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

struct patchInfo {
  uintptr_t address;
  std::vector<uint8_t> bytes;
};

namespace pl::patch {

PLAPI bool writeBytes(uintptr_t addr, const std::string &bytes_str,
                      const std::string &name);

PLAPI bool writeBytes(uintptr_t addr, const std::vector<uint8_t> &bytes,
                      const std::string &name);

PLAPI std::vector<uint8_t> readBytes(uintptr_t addr, size_t len);

PLAPI bool revert(const std::string &name);

PLAPI void revertAll();

} // namespace pl::patch
