#include "pl/memory/Patch.hpp"

#include <algorithm>
#include <cctype>
#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <sys/mman.h>
#include <unordered_map>
#include <unistd.h>
#include <vector>

namespace {
    struct PatchInfo {
        uintptr_t address;
        std::vector<uint8_t> bytes;
    };

    std::unordered_map<std::string, PatchInfo> patches;

    std::vector<uint8_t> readBytesImpl(uintptr_t addr, size_t len);

    static size_t getPageSize() {
        static size_t sz = sysconf(_SC_PAGESIZE);
        return sz;
    }

    static uintptr_t getPageStart(uintptr_t addr) {
        size_t ps = getPageSize();
        return addr & ~(ps - 1);
    }

    static bool checkedAddressRange(uintptr_t address, size_t length,
                                    uintptr_t &end) {
        if (address == 0 || length == 0)
            return false;
        if (length > std::numeric_limits<uintptr_t>::max() - address)
            return false;
        end = address + length;
        return true;
    }

    static bool hasReadableMappedRange(uintptr_t address, size_t length) {
        uintptr_t end = 0;
        if (!checkedAddressRange(address, length, end))
            return false;

        FILE *maps = std::fopen("/proc/self/maps", "r");
        if (!maps)
            return false;

        uintptr_t covered = address;
        char line[4096];
        while (std::fgets(line, sizeof(line), maps)) {
            uintptr_t start = 0;
            uintptr_t regionEnd = 0;
            char perms[5] = {};
            if (std::sscanf(line, "%" SCNxPTR "-%" SCNxPTR " %4s", &start,
                            &regionEnd, perms) != 3) {
                continue;
            }

            if (regionEnd <= covered || start > covered)
                continue;
            if (perms[0] != 'r')
                break;

            covered = std::min(regionEnd, end);
            if (covered == end) {
                std::fclose(maps);
                return true;
            }
        }

        std::fclose(maps);
        return false;
    }

    static bool setMemRWX(uintptr_t address, size_t length) {
        uintptr_t end = 0;
        if (!checkedAddressRange(address, length, end))
            return false;
        uintptr_t page_start = getPageStart(address);
        size_t page_size = getPageSize();
        size_t span = end - page_start;
        if (span > std::numeric_limits<size_t>::max() - (page_size - 1))
            return false;
        size_t page_count = (span + page_size - 1) / page_size;
        int ret = mprotect(reinterpret_cast<void *>(page_start),
                           page_count * page_size,
                           PROT_READ | PROT_WRITE | PROT_EXEC);
        if (ret != 0) {
            perror("mprotect");
            return false;
        }
        return true;
    }

    static int hexValue(unsigned char ch) {
        if (ch >= '0' && ch <= '9')
            return ch - '0';
        if (ch >= 'a' && ch <= 'f')
            return ch - 'a' + 10;
        if (ch >= 'A' && ch <= 'F')
            return ch - 'A' + 10;
        return -1;
    }

    static bool parseBytesString(const std::string &s, std::vector<uint8_t> &result) {
        result.clear();
        result.reserve((s.size() + 2) / 3);

        const char *cursor = s.c_str();
        while (*cursor) {
            while (*cursor &&
                   std::isspace(static_cast<unsigned char>(*cursor))) {
                ++cursor;
            }
            if (!*cursor) break;

            unsigned int byte = 0;
            int digits = 0;
            while (*cursor &&
                   !std::isspace(static_cast<unsigned char>(*cursor))) {
                if (digits == 2)
                    return false;

                const int nibble =
                    hexValue(static_cast<unsigned char>(*cursor));
                if (nibble < 0)
                    return false;
                byte = (byte << 4) | static_cast<unsigned int>(nibble);
                ++digits;
                ++cursor;
            }

            if (digits == 0)
                return false;
            result.push_back(static_cast<uint8_t>(byte));
        }

        return !result.empty();
    }

    bool writeBytesImpl(uintptr_t addr, const std::vector<uint8_t> &bytes, const std::string &name) {
        if (bytes.empty() || !hasReadableMappedRange(addr, bytes.size()))
            return false;
        std::vector<uint8_t> original = readBytesImpl(addr, bytes.size());
        if (original.size() != bytes.size())
            return false;
        if (!setMemRWX(addr, bytes.size()))
            return false;
        std::memcpy(reinterpret_cast<void *>(addr), bytes.data(), bytes.size());
        __builtin___clear_cache(reinterpret_cast<char *>(getPageStart(addr)),
                                reinterpret_cast<char *>(addr + bytes.size()));
        patches[name] = PatchInfo{addr, original};
        return true;
    }

    bool writeHexImpl(uintptr_t addr, const std::string &bytes_str, const std::string &name) {
        std::vector<uint8_t> bytes;
        if (!parseBytesString(bytes_str, bytes))
            return false;
        return writeBytesImpl(addr, bytes, name);
    }

    std::vector<uint8_t> readBytesImpl(uintptr_t addr, size_t len) {
        if (!hasReadableMappedRange(addr, len))
            return {};
        std::vector<uint8_t> out(len);
        std::memcpy(out.data(), reinterpret_cast<void *>(addr), len);
        return out;
    }

    bool revertImpl(const std::string &name) {
        auto it = patches.find(name);
        if (it == patches.end())
            return false;

        const PatchInfo &p = it->second;
        if (p.bytes.empty() || !hasReadableMappedRange(p.address, p.bytes.size()))
            return false;
        if (!setMemRWX(p.address, p.bytes.size()))
            return false;

        std::memcpy(reinterpret_cast<void *>(p.address), p.bytes.data(), p.bytes.size());
        __builtin___clear_cache(reinterpret_cast<char *>(getPageStart(p.address)),
                                reinterpret_cast<char *>(p.address + p.bytes.size()));
        patches.erase(it);
        return true;
    }

    void revertAllImpl() {
        for (auto &kv : patches) {
            const PatchInfo &p = kv.second;
            if (!p.bytes.empty() &&
                hasReadableMappedRange(p.address, p.bytes.size()) &&
                setMemRWX(p.address, p.bytes.size())) {
                std::memcpy(reinterpret_cast<void *>(p.address), p.bytes.data(), p.bytes.size());
                __builtin___clear_cache(reinterpret_cast<char *>(getPageStart(p.address)),
                                        reinterpret_cast<char *>(p.address + p.bytes.size()));
            }
        }
        patches.clear();
    }

} // namespace

namespace pl::memory {

bool writeBytes(uintptr_t addr, std::span<const uint8_t> bytes,
                std::string_view name) {
    return writeBytesImpl(addr, std::vector<uint8_t>(bytes.begin(), bytes.end()),
                          std::string(name));
}

bool writeBytes(uintptr_t addr, std::string_view bytes,
                std::string_view name) {
    return writeHexImpl(addr, std::string(bytes), std::string(name));
}

std::vector<uint8_t> readBytes(uintptr_t addr, size_t len) {
    return readBytesImpl(addr, len);
}

bool revertPatch(std::string_view name) {
    return revertImpl(std::string(name));
}

void revertAllPatches() {
    revertAllImpl();
}

} // namespace pl::memory
