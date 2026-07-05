#pragma once

/**
 * @file Signature.hpp
 * @brief Signature resolver API.
 */

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "pl/Export.hpp"

namespace pl::memory {

/**
 * @brief Resolves one byte signature inside a loaded module.
 */
PL_EXPORT uintptr_t resolveSignature(std::string_view signature,
                                     std::string_view moduleName);

/**
 * @brief Resolves multiple byte signatures inside a loaded module.
 */
PL_EXPORT std::unordered_map<std::string, uintptr_t>
resolveSignatures(std::span<const std::string> signatures,
                  std::string_view moduleName);

} // namespace pl::memory
