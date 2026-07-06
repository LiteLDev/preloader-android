#include "pl/legacy/LegacyPatch.h"

#include <cstring>
#include <span>
#include <string>
#include <vector>

#include "pl/memory/Patch.hpp"

namespace pl::patch {

PL_LEGACY_EXPORT bool writeBytes(uintptr_t addr,
                                 const std::vector<uint8_t> &bytes,
                                 const std::string &name) {
  return pl::memory::writeBytes(
      addr, std::span<const uint8_t>(bytes.data(), bytes.size()), name);
}

PL_LEGACY_EXPORT bool writeBytes(uintptr_t addr, const std::string &bytes,
                                 const std::string &name) {
  return pl::memory::writeBytes(addr, bytes, name);
}

PL_LEGACY_EXPORT std::vector<uint8_t> readBytes(uintptr_t addr, size_t len) {
  return pl::memory::readBytes(addr, len);
}

PL_LEGACY_EXPORT bool revert(const std::string &name) {
  return pl::memory::revertPatch(name);
}

PL_LEGACY_EXPORT void revertAll() { pl::memory::revertAllPatches(); }

} // namespace pl::patch

extern "C" {

PL_LEGACY_EXPORT bool pl_patch_write_bytes(uintptr_t addr, const uint8_t *bytes,
                                           size_t len, const char *name) {
  if (bytes == nullptr || name == nullptr || len == 0) {
    return false;
  }
  return pl::memory::writeBytes(addr, std::span<const uint8_t>(bytes, len),
                                name);
}

PL_LEGACY_EXPORT bool pl_patch_write_hex(uintptr_t addr, const char *bytes,
                                         const char *name) {
  if (bytes == nullptr || name == nullptr) {
    return false;
  }
  return pl::memory::writeBytes(addr, bytes, name);
}

PL_LEGACY_EXPORT size_t pl_patch_read_bytes(uintptr_t addr, uint8_t *out,
                                            size_t len) {
  if (out == nullptr || len == 0) {
    return 0;
  }

  const std::vector<uint8_t> bytes = pl::memory::readBytes(addr, len);
  if (bytes.size() != len) {
    return 0;
  }

  std::memcpy(out, bytes.data(), bytes.size());
  return bytes.size();
}

PL_LEGACY_EXPORT bool pl_patch_revert(const char *name) {
  if (name == nullptr) {
    return false;
  }
  return pl::memory::revertPatch(name);
}

PL_LEGACY_EXPORT void pl_patch_revert_all(void) {
  pl::memory::revertAllPatches();
}

} // extern "C"
