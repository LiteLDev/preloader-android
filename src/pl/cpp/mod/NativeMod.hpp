#pragma once

#include <memory>
#include <mutex>
#include <utility>

#include "pl/cpp/mod/Mod.hpp"

namespace pl::mod {

class NativeMod : public Mod {
public:
  using Mod::Mod;

  static NativeMod &createCurrent(JavaVM *vm, const PLModInfo *info) {
    std::lock_guard<std::mutex> lock(currentMutex());
    if (currentStorage())
      return *currentStorage();

    auto mod = std::make_shared<NativeMod>(vm, info);
    currentStorage() = std::move(mod);
    return *currentStorage();
  }

  static std::shared_ptr<NativeMod> current() {
    std::lock_guard<std::mutex> lock(currentMutex());
    return currentStorage();
  }

  static void clearCurrent() {
    std::lock_guard<std::mutex> lock(currentMutex());
    currentStorage().reset();
  }

private:
  static std::shared_ptr<NativeMod> &currentStorage() {
    static std::shared_ptr<NativeMod> instance;
    return instance;
  }

  static std::mutex &currentMutex() {
    static std::mutex mutex;
    return mutex;
  }
};

} // namespace pl::mod
