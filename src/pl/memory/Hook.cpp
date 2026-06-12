#include <memory>
#include <mutex>
#include <set>
#include <unordered_map>

#include "pl/Gloss.h"
#include "pl/c/Hook.h"

namespace pl::hook {

struct HookElement {
  PLFuncPtr detour{};
  PLFuncPtr *originalFunc{};
  int priority{};
  int id{};

  bool operator<(const HookElement &o) const noexcept {
    if (priority != o.priority) {
      return priority < o.priority;
    }
    return id < o.id;
  }
};

struct HookData {
  PLFuncPtr target{};
  PLFuncPtr origin{};
  PLFuncPtr start{};
  GHook glossHandle{};
  int counter{};
  std::set<HookElement> chain;

  ~HookData() {
    if (glossHandle) {
      GlossHookDelete(glossHandle);
    }
  }

  int nextId() noexcept { return ++counter; }

  void rebuildChain() {
    PLFuncPtr *prev = nullptr;
    for (auto &e : chain) {
      if (!prev) {
        start = e.detour;
        prev = e.originalFunc;
        *prev = origin;
      } else {
        *prev = e.detour;
        prev = e.originalFunc;
      }
    }

    if (prev) {
      *prev = origin;
    } else {
      start = origin;
    }

    if (glossHandle) {
      GlossHookReplaceNewFunc(glossHandle, start);
    }
  }
};

std::unordered_map<PLFuncPtr, std::shared_ptr<HookData>> &hooks() {
  static std::unordered_map<PLFuncPtr, std::shared_ptr<HookData>> m;
  return m;
}

std::mutex mtx;

int hook(PLFuncPtr target, PLFuncPtr detour, PLFuncPtr *original,
         int priority) {
  if (!target || !detour || !original) {
    return -1;
  }

  static bool inited = false;
  if (!inited) {
    GlossInit(true);
    inited = true;
  }

  std::lock_guard<std::mutex> lock(mtx);
  auto &map = hooks();
  auto it = map.find(target);

  if (it != map.end()) {
    auto h = it->second;
    h->chain.insert({detour, original, priority, h->nextId()});
    h->rebuildChain();
    return 0;
  }

  auto h = std::make_shared<HookData>();
  h->target = target;
  h->origin = target;

  h->glossHandle =
      GlossHook(reinterpret_cast<void *>(target), reinterpret_cast<void *>(detour),
                reinterpret_cast<void **>(&h->origin));
  if (!h->glossHandle) {
    return -1;
  }

  h->chain.insert({detour, original, priority, h->nextId()});
  h->rebuildChain();
  map[target] = h;
  return 0;
}

bool unhook(PLFuncPtr target, PLFuncPtr detour) {
  std::lock_guard<std::mutex> lock(mtx);
  auto &map = hooks();
  auto it = map.find(target);
  if (it == map.end()) {
    return false;
  }

  auto &h = it->second;
  bool removed = false;
  for (auto eit = h->chain.begin(); eit != h->chain.end(); ++eit) {
    if (eit->detour == detour) {
      h->chain.erase(eit);
      removed = true;
      break;
    }
  }

  if (!removed) {
    return false;
  }

  if (h->chain.empty()) {
    map.erase(it);
  } else {
    h->rebuildChain();
  }
  return true;
}

} // namespace pl::hook

extern "C" {

PLAPI int pl_hook(PLFuncPtr target, PLFuncPtr detour, PLFuncPtr *originalFunc,
                  PLHookPriority priority) {
  return pl::hook::hook(target, detour, originalFunc,
                        static_cast<int>(priority));
}

PLAPI bool pl_unhook(PLFuncPtr target, PLFuncPtr detour) {
  return pl::hook::unhook(target, detour);
}

} // extern "C"
