#include "Signature.h"
#include "Gloss.h"
#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace pl::signature {

struct SigPattern {
  std::vector<uint8_t> pattern;
  std::vector<bool> mask;
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
  for (const uint8_t *pos = base; pos <= end;) {
    size_t i = len - 1;
    while (i < len && (!sigpat.mask[i] || pos[i] == sigpat.pattern[i])) {
      if (i == 0)
        return pos;
      --i;
    }
    size_t skip = 1;
    if (sigpat.mask[len - 1])
      skip = bmhTable[pos[len - 1]];
    pos += skip;
  }
  return nullptr;
}

static const uint8_t *maskScan(const uint8_t *start, const uint8_t *end,
                               const SigPattern &pat) {
  for (const uint8_t *ptr = start; ptr <= end; ++ptr) {
    bool found = true;
    for (size_t i = 0; i < pat.pattern.size(); ++i) {
      if (pat.mask[i] && ptr[i] != pat.pattern[i]) {
        found = false;
        break;
      }
    }
    if (found)
      return ptr;
  }
  return nullptr;
}

// --- Cache structures ---
struct ModuleInfo {
  uintptr_t base = 0;
  size_t size = 0;
  GHandle handle = 0;
  bool initialized = false;
};

static std::unordered_map<std::string, ModuleInfo> moduleCache;
static std::unordered_map<std::string, uintptr_t> sigCache;
static std::unordered_map<std::string,
                          std::pair<SigPattern, std::vector<size_t>>>
    patternCache;
static std::mutex cacheMutex;

// --- Core function ---
uintptr_t resolveSignature(
    const std::string &signature,
    const std::string &moduleName /* default = libminecraftpe.so */) {
  const std::string combinedKey = moduleName + "::" + signature;

  { // check cache
    std::lock_guard<std::mutex> lock(cacheMutex);
    if (auto it = sigCache.find(combinedKey); it != sigCache.end())
      return it->second;
  }

  ModuleInfo mod;

  { // load module info
    std::lock_guard<std::mutex> lock(cacheMutex);
    auto it = moduleCache.find(moduleName);
    if (it != moduleCache.end()) {
      mod = it->second;
    } else {
      GlossInit(true);
      GHandle handle = GlossOpen(moduleName.c_str());
      if (!handle)
        return 0;
      mod.handle = handle;
      mod.base = GlossGetLibBiasEx(handle);
      mod.size = GlossGetLibFileSize(handle);
      mod.initialized = true;
      moduleCache[moduleName] = mod;
    }
  }

  uintptr_t addr = GlossSymbol(mod.handle, signature.c_str(), nullptr);
  if (addr) {
    std::lock_guard<std::mutex> lock(cacheMutex);
    sigCache[combinedKey] = addr;
    return addr;
  }

  // --- Parse pattern ---
  SigPattern sigpat;
  std::vector<size_t> bmhTable;
  {
    std::lock_guard<std::mutex> lock(cacheMutex);
    if (auto it = patternCache.find(signature); it != patternCache.end()) {
      sigpat = it->second.first;
      bmhTable = it->second.second;
    } else {
      for (size_t i = 0; i < signature.size();) {
        if (signature[i] == ' ') {
          ++i;
          continue;
        }
        if (signature[i] == '?') {
          sigpat.pattern.push_back(0);
          sigpat.mask.push_back(false);
          i += (i + 1 < signature.size() && signature[i + 1] == '?') ? 2 : 1;
        } else {
          if (i + 1 >= signature.size())
            break;
          char buf[3] = {signature[i], signature[i + 1], 0};
          unsigned long value = strtoul(buf, nullptr, 16);
          sigpat.pattern.push_back((uint8_t)value);
          sigpat.mask.push_back(true);
          i += 2;
        }
      }
      bmhTable = buildBMHTable(sigpat);
      patternCache[signature] = {sigpat, bmhTable};
    }
  }

  if (sigpat.pattern.empty())
    return mod.base;

  const uint8_t *start = reinterpret_cast<const uint8_t *>(mod.base);
  const uint8_t *end = start + mod.size - sigpat.pattern.size();

  const uint8_t *found_ptr = bmSearch(start, end, sigpat, bmhTable);
  if (!found_ptr)
    found_ptr = maskScan(start, end, sigpat);

  uintptr_t result = found_ptr ? reinterpret_cast<uintptr_t>(found_ptr) : 0;

  {
    std::lock_guard<std::mutex> lock(cacheMutex);
    sigCache[combinedKey] = result;
  }

  return result;
}

} // namespace signature