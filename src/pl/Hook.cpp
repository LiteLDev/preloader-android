#include <cstring>
#include <memory>
#include <mutex>
#include <set>
#include <unordered_map>

#include <dlfcn.h>
typedef enum {
  SHADOWHOOK_MODE_SHARED = 0,
  SHADOWHOOK_MODE_UNIQUE = 1
} shadowhook_mode_t;

static int (*shadowhook_init)(shadowhook_mode_t mode, bool debuggable);
static void *(*shadowhook_hook_func_addr)(void *func_addr, void *new_addr,
                                          void **orig_addr);
static int (*shadowhook_unhook)(void *stub);

namespace pl::hook {

using FuncPtr = void *;

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
  FuncPtr stub{};
  int hookId{};
  std::set<HookElement> hooks{};

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
            int priority) {
  std::lock_guard lock(hooksMutex);
  static bool init = false;

  if (!init) {
    void *handle = dlopen("libshadowhook.so", RTLD_LAZY);
    shadowhook_init =
        (int (*)(shadowhook_mode_t, bool))(dlsym(handle, "shadowhook_init"));

    shadowhook_hook_func_addr = (void *(*)(void *, void *, void **))(
        dlsym(handle, "shadowhook_hook_func_addr"));

    shadowhook_unhook =
        (int (*)(void *func_addr))(dlsym(handle, "shadowhook_unhook"));

    auto result = shadowhook_init(SHADOWHOOK_MODE_SHARED, false);
    if (result == 0) {
      init = true;
    }
  }
  
  auto it = getHooks().find(target);
  if (it != getHooks().end()) {
    auto hookData = it->second;
    hookData->hooks.insert(
        {detour, originalFunc, priority, hookData->incrementHookId()});
    hookData->updateCallList();

    shadowhook_hook_func_addr(target, hookData->start, &hookData->origin);
    return 0;
  }

  auto hookData = std::make_shared<HookData>();
  hookData->target = target;
  hookData->origin = nullptr;
  hookData->start = detour;
  hookData->hooks.insert(
      {detour, originalFunc, priority, hookData->incrementHookId()});
  hookData->stub = shadowhook_hook_func_addr(target, detour, &hookData->origin);
  if (!hookData->stub) {
    return -1;
  }

  hookData->updateCallList();
  getHooks().emplace(target, hookData);
  return 0;
}

bool pl_unhook(FuncPtr target, FuncPtr detour) {
  std::lock_guard lock(hooksMutex);

  if (!target)
    return false;

  auto hookDataIter = getHooks().find(target);
  if (hookDataIter == getHooks().end())
    return false;

  auto &hookData = hookDataIter->second;

  for (auto it = hookData->hooks.begin(); it != hookData->hooks.end(); ++it) {
    if (it->detour == detour) {
      hookData->hooks.erase(it);
      hookData->updateCallList();

      if (hookData->hooks.empty()) {
        shadowhook_unhook(hookData->stub);
        getHooks().erase(target);
      } else {
        shadowhook_hook_func_addr(target, hookData->start, &hookData->origin);
      }

      return true;
    }
  }

  return false;
}

} // namespace pl::hook