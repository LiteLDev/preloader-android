#include "Hook.h"
#include <cstdint>
#include <string>
#include <vector>
#include <mutex>
#include <set>
#include <type_traits>
#include <unordered_map>
#include <memory>

#include <dobby.h>

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
                Priority priority) {

        std::lock_guard lock(hooksMutex);
        auto it = getHooks().find(target);
        if (it != getHooks().end()) {
            auto hookData = it->second;
            hookData->hooks.insert(
                    {detour, originalFunc, priority, hookData->incrementHookId()});
            hookData->updateCallList();

            DobbyDestroy(target);
            if (DobbyHook((void *)target, (void *)hookData->start,
                          (void **)&hookData->origin) != 0) {
                return -1;
            }

            return 0;
        }

        auto hookData = std::make_shared<HookData>();
        hookData->target = target;
        hookData->origin = target;
        hookData->start = detour;
        hookData->hooks.insert(
                {detour, originalFunc, priority, hookData->incrementHookId()});

        if (DobbyHook((void *)target, (void *)hookData->start,
                      (void **)&hookData->origin) != 0) {
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

                DobbyDestroy(target);
                if (!hookData->hooks.empty()) {
                    DobbyHook((void *)target, (void *)hookData->start,
                              (void **)&hookData->origin);
                } else {
                    getHooks().erase(target);
                }

                return true;
            }
        }
        return false;
    }

} // namespace pl::hook