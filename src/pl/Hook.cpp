#include "pl/Gloss.h"
#include "pl/Hook.h"
#include <mutex>
#include <memory>
#include <unordered_map>
#include <set>

namespace pl::hook {

    struct HookElement {
        FuncPtr detour{};
        FuncPtr *originalFunc{};
        int priority{};
        int id{};

        bool operator<(const HookElement &o) const noexcept {
            if (priority != o.priority)
                return priority < o.priority;
            return id < o.id;
        }
    };

    struct HookData {
        FuncPtr target{};
        FuncPtr origin{};
        FuncPtr start{};
        GHook glossHandle{};
        int counter{};
        std::set<HookElement> chain;

        ~HookData() {
            if (glossHandle)
                GlossHookDelete(glossHandle);
        }

        int nextId() noexcept { return ++counter; }

        void rebuildChain() {
            FuncPtr *prev = nullptr;
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

            if (prev)
                *prev = origin;
            else
                start = origin;

            if (glossHandle)
                GlossHookReplaceNewFunc(glossHandle, start);
        }
    };

    static std::unordered_map<FuncPtr, std::shared_ptr<HookData>>& hooks() {
        static std::unordered_map<FuncPtr, std::shared_ptr<HookData>> m;
        return m;
    }
    static std::mutex mtx;

    int pl_hook(FuncPtr target, FuncPtr detour, FuncPtr *original, Priority priority) {
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

        h->glossHandle = GlossHook(reinterpret_cast<void*>(target),
                                   reinterpret_cast<void*>(detour),
                                   reinterpret_cast<void**>(&h->origin));
        if (!h->glossHandle)
            return -1;

        h->chain.insert({detour, original, priority, h->nextId()});
        h->rebuildChain();
        map[target] = h;
        return 0;
    }

    bool pl_unhook(FuncPtr target, FuncPtr detour) {
        std::lock_guard<std::mutex> lock(mtx);
        auto &map = hooks();
        auto it = map.find(target);
        if (it == map.end()) return false;

        auto &h = it->second;
        bool removed = false;
        for (auto eit = h->chain.begin(); eit != h->chain.end(); ++eit) {
            if (eit->detour == detour) {
                h->chain.erase(eit);
                removed = true;
                break;
            }
        }

        if (!removed) return false;

        if (h->chain.empty()) {
            map.erase(it);
        } else {
            h->rebuildChain();
        }
        return true;
    }
    
    bool pl_vhook(const char* cls, int slot, FuncPtr* outOrig, FuncPtr hookFn) {
        size_t rodataSize = 0;
        uintptr_t rodata = GlossGetLibSection(MCPE_LIB, ".rodata", &rodataSize);
        if (!rodata || !rodataSize) { logger.error("Hook: no .rodata for %s", cls); return false; }
    
        auto scan = [](uintptr_t base, size_t sz, const void* pat, size_t plen) -> uintptr_t {
            auto* m = (const uint8_t*)base; auto* p = (const uint8_t*)pat;
            for (size_t i = 0; i+plen <= sz; ++i)
                if (memcmp(m+i, p, plen) == 0) return base+i;
            return 0;
        };
    
        uintptr_t zts = scan(rodata, rodataSize, cls, strlen(cls)+1);
        if (!zts) { logger.error("Hook: ZTS not found for %s", cls); return false; }
    
        size_t drrSize = 0;
        uintptr_t drr = GlossGetLibSection(MCPE_LIB, ".data.rel.ro", &drrSize);
        if (!drr || !drrSize) { logger.error("Hook: no .data.rel.ro for %s", cls); return false; }
    
        uintptr_t zti = 0;
        for (size_t i = 0; i+sizeof(uintptr_t) <= drrSize; i += sizeof(uintptr_t)) {
            if (*reinterpret_cast<uintptr_t*>(drr+i) == zts) { zti = drr+i-sizeof(uintptr_t); break; }
        }
        if (!zti) { logger.error("Hook: ZTI not found for %s", cls); return false; }
    
        uintptr_t vtbl = 0;
        for (size_t i = 0; i+sizeof(uintptr_t) <= drrSize; i += sizeof(uintptr_t)) {
            if (*reinterpret_cast<uintptr_t*>(drr+i) == zti) { vtbl = drr+i+sizeof(uintptr_t); break; }
        }
        if (!vtbl) { logger.error("Hook: VTable not found for %s", cls); return false; }
    
        void** vt = reinterpret_cast<void**>(vtbl);
        *outOrig = vt[slot];
        Unprotect(vtbl + slot*sizeof(void*), sizeof(void*));
        vt[slot] = hookFn;
        __builtin___clear_cache((char*)(vtbl+slot*sizeof(void*)), (char*)(vtbl+(slot+1)*sizeof(void*)));
        logger.info("hooked %s slot[%d]", cls, slot);
        return true;
    }

} // namespace pl::hook