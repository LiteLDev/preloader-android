#include "pl/Mod.hpp"

namespace {
thread_local pl::mod::NativeMod *gCurrentMod{};
}

namespace pl::mod {

NativeMod *NativeMod::current() noexcept { return gCurrentMod; }

namespace detail {

ScopedCurrentMod::ScopedCurrentMod(NativeMod *current) noexcept
    : mPrevious(gCurrentMod) {
  gCurrentMod = current;
}

ScopedCurrentMod::~ScopedCurrentMod() { gCurrentMod = mPrevious; }

} // namespace detail

} // namespace pl::mod
