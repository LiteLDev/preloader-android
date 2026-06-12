#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "pl/c/Patch.h"

namespace pl::patch {

inline bool writeBytes(uintptr_t addr, const std::string &bytes,
                       const std::string &name) {
  return ::pl_patch_write_hex(addr, bytes.c_str(), name.c_str());
}

inline bool writeBytes(uintptr_t addr, const std::vector<uint8_t> &bytes,
                       const std::string &name) {
  return ::pl_patch_write_bytes(addr, bytes.data(), bytes.size(),
                                name.c_str());
}

inline std::vector<uint8_t> readBytes(uintptr_t addr, size_t len) {
  if (len == 0) {
    return {};
  }

  std::vector<uint8_t> out(len);
  const size_t read = ::pl_patch_read_bytes(addr, out.data(), out.size());
  if (read == 0) {
    return {};
  }

  out.resize(read);
  return out;
}

inline bool revert(const std::string &name) {
  return ::pl_patch_revert(name.c_str());
}

inline void revertAll() {
  ::pl_patch_revert_all();
}

} // namespace pl::patch
