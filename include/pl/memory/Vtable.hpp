#pragma once

/**
 * @file Vtable.hpp
 * @brief RTTI-backed vtable resolver API.
 */

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "pl/Export.hpp"

namespace pl::memory {

/**
 * @brief Resolves the function pointer stored in a primary vtable slot.
 *
 * @param typeInfoName Itanium ABI RTTI typeinfo-name object symbol
 *        ("_ZTS9CameraAPI") or its stored name string ("9CameraAPI").
 * @param slot Function slot index from the vtable address point.
 * @param moduleName Loaded library name or path.
 * @return Function pointer stored in the slot, or 0 when it cannot be resolved.
 */
PL_EXPORT uintptr_t resolveVtableFunction(std::string_view typeInfoName,
                                          size_t slot,
                                          std::string_view moduleName);

} // namespace pl::memory
