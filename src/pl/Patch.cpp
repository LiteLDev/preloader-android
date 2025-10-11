#include "Patch.h"
#include <cstdio>
#include <cstring>
#include <sstream>
#include <sys/mman.h>
#include <unistd.h>

namespace pl::patch {
    std::unordered_map<std::string, patchInfo> patches;

    static size_t getPageSize() {
        static size_t sz = sysconf(_SC_PAGESIZE);
        return sz;
    }

    static uintptr_t getPageStart(uintptr_t addr) {
        size_t ps = getPageSize();
        return addr & ~(ps - 1);
    }

    static bool setMemRWX(uintptr_t address, size_t length) {
        uintptr_t page_start = getPageStart(address);
        size_t page_size = getPageSize();
        size_t page_count = ((address + length - page_start) + page_size - 1) / page_size;
        int ret = mprotect(reinterpret_cast<void *>(page_start),
                           page_count * page_size,
                           PROT_READ | PROT_WRITE | PROT_EXEC);
        if (ret != 0) {
            perror("mprotect");
            return false;
        }
        return true;
    }

    static std::vector<uint8_t> parseBytesString(const std::string &s) {
        std::vector<uint8_t> result;
        std::istringstream iss(s);
        std::string token;
        while (iss >> token) {
            unsigned int byte;
            if (sscanf(token.c_str(), "%x", &byte) == 1 && byte <= 0xFF)
                result.push_back(static_cast<uint8_t>(byte));
        }
        return result;
    }

    bool writeBytes(uintptr_t addr, const std::vector<uint8_t> &bytes, const std::string &name) {
        if (bytes.empty())
            return false;
        std::vector<uint8_t> original = readBytes(addr, bytes.size());
        if (!setMemRWX(addr, bytes.size()))
            return false;
        std::memcpy(reinterpret_cast<void *>(addr), bytes.data(), bytes.size());
        __builtin___clear_cache(reinterpret_cast<char *>(getPageStart(addr)),
                                reinterpret_cast<char *>(addr + bytes.size()));
        patches[name] = patchInfo{addr, original};
        return true;
    }

    bool writeBytes(uintptr_t addr, const std::string &bytes_str, const std::string &name) {
        std::vector<uint8_t> bytes = parseBytesString(bytes_str);
        return writeBytes(addr, bytes, name);
    }

    std::vector<uint8_t> readBytes(uintptr_t addr, size_t len) {
        std::vector<uint8_t> out(len);
        std::memcpy(out.data(), reinterpret_cast<void *>(addr), len);
        return out;
    }

    bool revert(const std::string &name) {
        auto it = patches.find(name);
        if (it == patches.end())
            return false;

        const patchInfo &p = it->second;
        if (!setMemRWX(p.address, p.bytes.size()))
            return false;

        std::memcpy(reinterpret_cast<void *>(p.address), p.bytes.data(), p.bytes.size());
        __builtin___clear_cache(reinterpret_cast<char *>(getPageStart(p.address)),
                                reinterpret_cast<char *>(p.address + p.bytes.size()));
        patches.erase(it);
        return true;
    }

    void revertAll() {
        for (auto &kv : patches) {
            const patchInfo &p = kv.second;
            if (setMemRWX(p.address, p.bytes.size())) {
                std::memcpy(reinterpret_cast<void *>(p.address), p.bytes.data(), p.bytes.size());
                __builtin___clear_cache(reinterpret_cast<char *>(getPageStart(p.address)),
                                        reinterpret_cast<char *>(p.address + p.bytes.size()));
            }
        }
        patches.clear();
    }

} // namespace pl::patch