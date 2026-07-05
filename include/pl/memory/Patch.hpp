#pragma once

/**
 * @file Patch.hpp
 * @brief Memory patch API.
 */

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "pl/Export.hpp"

namespace pl::memory {

/**
 * @brief Writes raw bytes to an address and stores the original bytes by name.
 */
PL_EXPORT bool writeBytes(uintptr_t address, std::span<const uint8_t> bytes,
                          std::string_view name);

/**
 * @brief Writes bytes encoded as a hex string and stores the patch by name.
 */
PL_EXPORT bool writeBytes(uintptr_t address, std::string_view hexBytes,
                          std::string_view name);

/**
 * @brief Reads bytes from an address.
 */
PL_EXPORT std::vector<uint8_t> readBytes(uintptr_t address, size_t length);

/**
 * @brief Reverts a named patch.
 */
PL_EXPORT bool revertPatch(std::string_view name);

/**
 * @brief Reverts every patch created through this API.
 */
PL_EXPORT void revertAllPatches();

/**
 * @brief RAII owner for a named patch.
 */
class PatchHandle {
public:
  PatchHandle() = default;

  PatchHandle(uintptr_t address, std::span<const uint8_t> bytes,
              std::string name)
      : mName(std::move(name)), mApplied(writeBytes(address, bytes, mName)) {}

  PatchHandle(uintptr_t address, std::string_view hexBytes, std::string name)
      : mName(std::move(name)), mApplied(writeBytes(address, hexBytes, mName)) {
  }

  PatchHandle(const PatchHandle &) = delete;
  PatchHandle &operator=(const PatchHandle &) = delete;

  PatchHandle(PatchHandle &&other) noexcept { swap(other); }

  PatchHandle &operator=(PatchHandle &&other) noexcept {
    if (this != &other) {
      reset();
      swap(other);
    }
    return *this;
  }

  ~PatchHandle() { reset(); }

  [[nodiscard]] bool applied() const noexcept { return mApplied; }

  void reset() {
    if (mApplied) {
      revertPatch(mName);
      mApplied = false;
    }
    mName.clear();
  }

  void swap(PatchHandle &other) noexcept {
    std::swap(mName, other.mName);
    std::swap(mApplied, other.mApplied);
  }

private:
  std::string mName;
  bool mApplied{};
};

} // namespace pl::memory
