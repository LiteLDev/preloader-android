#include "Signature.h"
#include <cstdint>
#include <dlfcn.h>
#include <fstream>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>
#include <sstream>
#include <sys/stat.h>
#include "Logger.h"

namespace pl::signature {

    struct SigPattern {
        std::vector<uint8_t> pattern;
        std::vector<bool> mask;
    };

    struct ModuleInfo {
        uintptr_t base = 0;
        size_t size = 0;
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

    static const uint8_t *bmSearch(const uint8_t *base, const uint8_t *end,
                                   const SigPattern &sigpat,
                                   const std::vector<size_t> &bmhTable) {
        const size_t len = sigpat.pattern.size();
        if (len == 0 || end <= base) return nullptr;

        for (const uint8_t *pos = base; pos <= end;) {
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

    static const uint8_t *maskScan(const uint8_t *start, const uint8_t *end, const SigPattern &pat) {
        const size_t patSize = pat.pattern.size();
        if (patSize == 0) return nullptr;

        for (const uint8_t *ptr = start; ptr <= end; ++ptr) {
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


    static bool getModuleInfo(const std::string &name, ModuleInfo &out) {
        std::ifstream maps("/proc/self/maps");
        if (!maps.is_open()) return false;

        std::string line;
        while (std::getline(maps, line)) {
            if (line.find(name) == std::string::npos) continue;

            std::istringstream iss(line);
            std::string addr, perms, offset, dev, inode, path;
            iss >> addr >> perms >> offset >> dev >> inode;
            std::getline(iss, path);

            uintptr_t start = 0, end = 0;
            if (sscanf(addr.c_str(), "%lx-%lx", &start, &end) != 2) continue;
            if (end <= start) continue;

            out.base = start;
            out.size = end - start;
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
        return false;
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
        std::string combinedKey;
        combinedKey.reserve(strlen(moduleName) + strlen(signature) + 2);
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

        if (sigpat.pattern.empty()) return mod.base;

        const uint8_t *start = reinterpret_cast<const uint8_t *>(mod.base);
        const uint8_t *end = start + mod.size - sigpat.pattern.size();

        const uint8_t *foundPtr = bmSearch(start, end, sigpat, table);
        if (!foundPtr)
            foundPtr = maskScan(start, end, sigpat);

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


