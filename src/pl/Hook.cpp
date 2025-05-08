#include "Hook.h"

#include <cstdint>
#include <string>
#include <vector>

#ifndef RT_SUCCESS
#define RT_SUCCESS 0
#endif

#include <mutex>
#include <set>
#include <type_traits>
#include <unordered_map>

#include <dlfcn.h>

#include "Gloss.h"

namespace pl::hook {

struct HookElement {
  FuncPtr detour{};
  FuncPtr *originalFunc{};
  int priority{};
  int id{};

  bool operator<(const HookElement &other) const {
    if (priority != other.priority)
      return priority < other.priority;
    return id < other.id;
  }
};

struct HookData {
  FuncPtr target{};
  FuncPtr origin{};
  FuncPtr start{};
  GHook glossHandle{};
  int hookId{};
  std::set<HookElement> hooks{};

  ~HookData() {
    if (glossHandle != nullptr) {
      GlossHookDelete(glossHandle);
    }
  }

  void updateCallList() {
    FuncPtr *last = nullptr;
    for (auto &item : this->hooks) {
      if (last == nullptr) {
        this->start = item.detour;
        last = item.originalFunc;
        *last = this->origin;
      } else {
        *last = item.detour;
        last = item.originalFunc;
      }
    }
    if (last == nullptr) {
      this->start = this->origin;
    } else {
      *last = this->origin;
    }
  }

  int incrementHookId() { return ++hookId; }
};

std::unordered_map<FuncPtr, std::shared_ptr<HookData>> &getHooks() {
  static std::unordered_map<FuncPtr, std::shared_ptr<HookData>> hooks;
  return hooks;
}

static std::mutex hooksMutex{};

int pl_hook(FuncPtr target, FuncPtr detour, FuncPtr *originalFunc,
            Priority priority) {

  static bool initialized = false;
  if (!initialized) {
    GlossInit(true);
    initialized = true;
  }

  std::lock_guard lock(hooksMutex);
  auto it = getHooks().find(target);
  if (it != getHooks().end()) {
    auto hookData = it->second;
    hookData->hooks.insert(
        {detour, originalFunc, priority, hookData->incrementHookId()});
    hookData->updateCallList();

    if (hookData->glossHandle) {
      GlossHookReplaceNewFunc(hookData->glossHandle, hookData->start);
    }
    return 0;
  }

  auto hookData = std::make_shared<HookData>();
  hookData->target = target;
  hookData->origin = target;
  hookData->start = detour;
  hookData->hooks.insert(
      {detour, originalFunc, priority, hookData->incrementHookId()});

  hookData->glossHandle = GlossHook(target, hookData->start, &hookData->origin);

  if (!hookData->glossHandle) {
    return -1;
  }

  hookData->updateCallList();
  getHooks().emplace(target, hookData);
  return 0;
}

bool pl_unhook(FuncPtr target, FuncPtr detour) {
  std::lock_guard lock(hooksMutex);

  if (target == nullptr) {
    return false;
  }

  auto hookDataIter = getHooks().find(target);
  if (hookDataIter == getHooks().end()) {
    return false;
  }

  auto &hookData = hookDataIter->second;
  for (auto it = hookData->hooks.begin(); it != hookData->hooks.end(); ++it) {
    if (it->detour == detour) {
      hookData->hooks.erase(it);
      hookData->updateCallList();

      if (hookData->glossHandle) {
        if (hookData->hooks.empty()) {
          GlossHookDelete(hookData->glossHandle);
          getHooks().erase(target);
        } else {
          GlossHookReplaceNewFunc(hookData->glossHandle, hookData->start);
        }
      }
      return true;
    }
  }
  return false;
}

} // namespace pl::hook