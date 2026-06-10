#include "Signature.h"

#include <cinttypes>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <sys/stat.h>
#include <unordered_map>
#include <vector>

#include "Logger.h"

namespace pl::signature {

    struct SigPattern {
        std::vector<uint8_t> pattern;
        std::vector<bool> mask;
    };

    struct MemoryRegion {
        uintptr_t start = 0;
        size_t size = 0;
    };

    struct ModuleInfo {
        std::vector<MemoryRegion> regions;
        void *handle = nullptr;
        bool initialized = false;
    };

    static std::vector<size_t> buildBMHTable(const SigPattern &sigpat) {
        std::vector<size_t> table(256, sigpat.pattern.size());
        for (size_t i = 0; i + 1 < sigpat.pattern.size(); ++i) {
            if (sigpat.mask[i]) {
                table[sigpat.pattern[i]] = sigpat.pattern.size() - 1 - i;
            }
        }
        return table;
    }

    static const uint8_t *bmSearch(const uint8_t *base, size_t size,
                                   const SigPattern &sigpat,
                                   const std::vector<size_t> &bmhTable) {
        const size_t len = sigpat.pattern.size();
        if (len == 0 || size < len) return nullptr;

        const uint8_t *last = base + size - len;
        for (const uint8_t *pos = base; pos <= last;) {
            size_t i = len - 1;
            while (i < len && (!sigpat.mask[i] || pos[i] == sigpat.pattern[i])) {
                if (i == 0) return pos;
                --i;
            }
            size_t skip = sigpat.mask[len - 1] ? bmhTable[pos[len - 1]] : 1;
            pos += skip;
        }
        return nullptr;
    }

    static const uint8_t *maskScan(const uint8_t *start, size_t size, const SigPattern &pat) {
        const size_t patSize = pat.pattern.size();
        if (patSize == 0 || size < patSize) return nullptr;

        const uint8_t *last = start + size - patSize;
        for (const uint8_t *ptr = start; ptr <= last; ++ptr) {
            bool matched = true;
            for (size_t i = 0; i < patSize; ++i) {
                if (pat.mask[i] && ptr[i] != pat.pattern[i]) {
                    matched = false;
                    break;
                }
            }
            if (matched)
                return ptr;
        }
        return nullptr;
    }


    static bool isReadableMapping(const char *perms) {
        return perms && perms[0] == 'r';
    }

    static void addReadableRegion(ModuleInfo &module, uintptr_t start, uintptr_t end) {
        if (!module.regions.empty()) {
            MemoryRegion &last = module.regions.back();
            if (last.start + last.size == start) {
                last.size += static_cast<size_t>(end - start);
                return;
            }
        }

        module.regions.push_back({start, static_cast<size_t>(end - start)});
    }

    static bool getModuleInfo(const std::string &name, ModuleInfo &out) {
        FILE *maps = std::fopen("/proc/self/maps", "r");
        if (!maps) return false;

        char line[4096];
        while (std::fgets(line, sizeof(line), maps)) {
            if (std::strstr(line, name.c_str()) == nullptr) continue;

            uintptr_t start = 0, end = 0;
            char perms[5] = {};
            if (std::sscanf(line, "%" SCNxPTR "-%" SCNxPTR " %4s",
                            &start, &end, perms) != 3) continue;
            if (end <= start) continue;

            if (!isReadableMapping(perms)) continue;

            addReadableRegion(out, start, end);
        }
        std::fclose(maps);

        if (out.regions.empty()) return false;

        out.handle = dlopen(name.c_str(), RTLD_LAZY | RTLD_NOLOAD);
        if (!out.handle) {
            out.handle = dlopen(name.c_str(), RTLD_LAZY);
            if (!out.handle) {
                return false;
            }
        }

        out.initialized = true;
        return true;
    }

    static const uint8_t *scanRegion(const MemoryRegion &region, const SigPattern &sigpat,
                                     const std::vector<size_t> &table) {
        const uint8_t *start = reinterpret_cast<const uint8_t *>(region.start);
        const uint8_t *foundPtr = bmSearch(start, region.size, sigpat, table);
        if (!foundPtr)
            foundPtr = maskScan(start, region.size, sigpat);
        return foundPtr;
    }

    static std::unordered_map<std::string, ModuleInfo> moduleCache;
    static std::unordered_map<std::string, uintptr_t> sigCache;
    static std::unordered_map<std::string, std::pair<SigPattern, std::vector<size_t>>> patternCache;
    static std::shared_mutex cacheMutex;

    static SigPattern parsePattern(const std::string &signature) {
        SigPattern pat;
        for (size_t i = 0; i < signature.size();) {
            if (signature[i] == ' ') {
                ++i;
                continue;
            } else if (signature[i] == '?') {
                pat.pattern.push_back(0);
                pat.mask.push_back(false);
                i += (i + 1 < signature.size() && signature[i + 1] == '?') ? 2 : 1;
            } else {
                if (i + 1 >= signature.size()) break;
                char buf[3] = {signature[i], signature[i + 1], 0};
                uint8_t val = static_cast<uint8_t>(strtoul(buf, nullptr, 16));
                pat.pattern.push_back(val);
                pat.mask.push_back(true);
                i += 2;
            }
        }
        return pat;
    }

    uintptr_t pl_resolve_signature(const char* signature, const char* moduleName) {
        if (!signature || !moduleName) return 0;

        std::string combinedKey;
        combinedKey.reserve(std::strlen(moduleName) + std::strlen(signature) + 2);
        combinedKey.append(moduleName).append("::").append(signature);

        {
            std::shared_lock lk(cacheMutex);
            if (auto it = sigCache.find(combinedKey); it != sigCache.end())
                return it->second;
        }

        ModuleInfo mod;
        {
            std::unique_lock lk(cacheMutex);
            if (auto it = moduleCache.find(moduleName); it != moduleCache.end())
                mod = it->second;
            else if (getModuleInfo(moduleName, mod))
                moduleCache[moduleName] = mod;
            else
                return 0;
        }

        if (mod.handle) {
            if (void *sym = dlsym(mod.handle, signature)) {
                uintptr_t addr = reinterpret_cast<uintptr_t>(sym);
                std::unique_lock lk(cacheMutex);
                sigCache[combinedKey] = addr;
                return addr;
            }
        }

        SigPattern sigpat;
        std::vector<size_t> table;
        {
            std::unique_lock lk(cacheMutex);
            if (auto it = patternCache.find(signature); it != patternCache.end()) {
                sigpat = it->second.first;
                table = it->second.second;
            } else {
                sigpat = parsePattern(signature);
                table = buildBMHTable(sigpat);
                patternCache[signature] = {sigpat, table};
            }
        }

        if (sigpat.pattern.empty())
            return mod.regions.empty() ? 0 : mod.regions.front().start;

        const uint8_t *foundPtr = nullptr;
        for (const auto &region : mod.regions) {
            foundPtr = scanRegion(region, sigpat, table);
            if (foundPtr) break;
        }

        uintptr_t result = foundPtr ? reinterpret_cast<uintptr_t>(foundPtr) : 0;

        {
            std::unique_lock lk(cacheMutex);
            sigCache[combinedKey] = result;
        }

        return result;
    }

    uintptr_t resolveSignature(const std::string &signature, const std::string &moduleName) {
        return pl_resolve_signature(signature.c_str(), moduleName.c_str());
    }
} // namespace pl::signature


