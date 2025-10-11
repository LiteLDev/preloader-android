#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

struct patchInfo {
    uintptr_t address;
    std::vector<uint8_t> bytes;
};

namespace pl::patch {

    bool writeBytes(uintptr_t addr, const std::string &bytes_str, const std::string &name);

    bool writeBytes(uintptr_t addr, const std::vector<uint8_t> &bytes, const std::string &name);

    std::vector<uint8_t> readBytes(uintptr_t addr, size_t len);

    bool revert(const std::string &name);

    void revertAll();

} // namespace pl::patch