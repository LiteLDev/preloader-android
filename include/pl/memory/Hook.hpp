#pragma once

/**
 * @file Hook.hpp
 * @brief Memory hook API.
 */

#include <utility>

#include "pl/Export.hpp"

namespace pl::memory {

using FuncPtr = void *;

/**
 * @brief Hook chain priority. Lower values run earlier.
 */
enum class HookPriority : int {
  Highest = 0,
  High = 100,
  Normal = 200,
  Low = 300,
  Lowest = 400,
};

/**
 * @brief Installs a detour for a target function.
 */
PL_EXPORT int hook(FuncPtr target, FuncPtr detour, FuncPtr *originalFunc,
                   HookPriority priority = HookPriority::Normal);

/**
 * @brief Removes a detour from a target function.
 */
PL_EXPORT bool unhook(FuncPtr target, FuncPtr detour);

/**
 * @brief RAII owner for an installed hook.
 */
class HookHandle {
public:
  HookHandle() = default;

  HookHandle(FuncPtr target, FuncPtr detour, FuncPtr *originalFunc,
             HookPriority priority = HookPriority::Normal)
      : mTarget(target), mDetour(detour),
        mInstalled(hook(target, detour, originalFunc, priority) == 0) {}

  HookHandle(const HookHandle &) = delete;
  HookHandle &operator=(const HookHandle &) = delete;

  HookHandle(HookHandle &&other) noexcept { swap(other); }

  HookHandle &operator=(HookHandle &&other) noexcept {
    if (this != &other) {
      reset();
      swap(other);
    }
    return *this;
  }

  ~HookHandle() { reset(); }

  [[nodiscard]] bool installed() const noexcept { return mInstalled; }

  void reset() {
    if (mInstalled) {
      unhook(mTarget, mDetour);
      mInstalled = false;
    }
    mTarget = nullptr;
    mDetour = nullptr;
  }

  void swap(HookHandle &other) noexcept {
    std::swap(mTarget, other.mTarget);
    std::swap(mDetour, other.mDetour);
    std::swap(mInstalled, other.mInstalled);
  }

private:
  FuncPtr mTarget{};
  FuncPtr mDetour{};
  bool mInstalled{};
};

} // namespace pl::memory

