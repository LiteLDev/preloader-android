#include "pl/memory/Signature.hpp"

#include <algorithm>
#include <cctype>
#include <cinttypes>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <dlfcn.h>
#include <mutex>
#include <span>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "pl/Logger.hpp"

namespace pl::memory {
namespace {

constexpr size_t kMaxExactAnchorSize = 8;

struct PatternByte {
  uint8_t value = 0;
  uint8_t mask = 0;
};

struct ParsedPattern {
  std::vector<PatternByte> bytes;
  std::vector<size_t> checkIndices;
  size_t anchorIndex = 0;
  size_t anchorSize = 1;
};

struct MemoryRegion {
  uintptr_t start = 0;
  uintptr_t end = 0;
};

struct ModuleInfo {
  std::vector<MemoryRegion> regions;
  void *handle = nullptr;
};

struct CompiledPattern {
  std::string signature;
  ParsedPattern pattern;
};

std::unordered_map<std::string, ModuleInfo> moduleCache;
std::unordered_map<std::string, uintptr_t> sigCache;
std::unordered_map<std::string, ParsedPattern> patternCache;
std::shared_mutex cacheMutex;

int hexValue(char ch) {
  if (ch >= '0' && ch <= '9') return ch - '0';
  if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
  if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
  return -1;
}

bool parsePatternToken(std::string_view token, PatternByte &byte) {
  if (token == "?" || token == "??") {
    byte = PatternByte{0, 0};
    return true;
  }
  if (token.size() != 2) return false;

  uint8_t value = 0;
  uint8_t mask = 0;
  for (size_t i = 0; i < token.size(); ++i) {
    const char ch = token[i];
    const auto shift = static_cast<uint8_t>((1 - i) * 4);
    if (ch == '?') continue;
    const int digit = hexValue(ch);
    if (digit < 0) return false;
    value |= static_cast<uint8_t>(digit << shift);
    mask |= static_cast<uint8_t>(0xF << shift);
  }

  byte = PatternByte{value, mask};
  return true;
}

void appendPatternByte(ParsedPattern &pattern, PatternByte byte) {
  const size_t index = pattern.bytes.size();
  if (byte.mask != 0) pattern.checkIndices.push_back(index);
  pattern.bytes.push_back(byte);
}

bool appendPatternToken(std::string_view token, ParsedPattern &pattern) {
  PatternByte byte{};
  if (parsePatternToken(token, byte)) {
    appendPatternByte(pattern, byte);
    return true;
  }
  if (token.empty() || token.size() % 2 != 0) return false;

  for (size_t pos = 0; pos < token.size(); pos += 2) {
    if (!parsePatternToken(token.substr(pos, 2), byte)) return false;
    appendPatternByte(pattern, byte);
  }
  return true;
}

ParsedPattern parsePattern(std::string_view signature) {
  ParsedPattern pattern;
  size_t pos = 0;
  while (pos < signature.size()) {
    while (pos < signature.size() &&
           std::isspace(static_cast<unsigned char>(signature[pos]))) {
      ++pos;
    }
    if (pos >= signature.size()) break;

    const size_t start = pos;
    while (pos < signature.size() &&
           !std::isspace(static_cast<unsigned char>(signature[pos]))) {
      ++pos;
    }
    if (!appendPatternToken(signature.substr(start, pos - start), pattern)) {
      pattern.bytes.clear();
      pattern.checkIndices.clear();
      break;
    }
  }
  return pattern;
}

bool matches(PatternByte pattern, uint8_t value) {
  return (value & pattern.mask) == pattern.value;
}

bool isExactByte(PatternByte byte) { return byte.mask == 0xFF; }

int maskBits(uint8_t mask) {
  int count = 0;
  while (mask != 0) {
    mask &= static_cast<uint8_t>(mask - 1);
    ++count;
  }
  return count;
}

void selectAnchor(ParsedPattern &pattern) {
  if (pattern.checkIndices.empty()) return;

  size_t bestStart = 0;
  size_t bestSize = 0;
  for (size_t runStart = 0; runStart < pattern.bytes.size();) {
    if (!isExactByte(pattern.bytes[runStart])) {
      ++runStart;
      continue;
    }

    size_t runEnd = runStart + 1;
    while (runEnd < pattern.bytes.size() &&
           isExactByte(pattern.bytes[runEnd])) {
      ++runEnd;
    }

    const size_t size = std::min(runEnd - runStart, kMaxExactAnchorSize);
    const size_t start = runEnd - size;
    if (size > bestSize || (size == bestSize && start > bestStart)) {
      bestStart = start;
      bestSize = size;
    }
    runStart = runEnd;
  }

  if (bestSize != 0) {
    pattern.anchorIndex = bestStart;
    pattern.anchorSize = bestSize;
    return;
  }

  size_t bestIndex = pattern.checkIndices.front();
  int bestBits = maskBits(pattern.bytes[bestIndex].mask);
  for (const size_t index : pattern.checkIndices) {
    const int bits = maskBits(pattern.bytes[index].mask);
    if (bits > bestBits || (bits == bestBits && index > bestIndex)) {
      bestIndex = index;
      bestBits = bits;
    }
  }
  pattern.anchorIndex = bestIndex;
  pattern.anchorSize = 1;
}

bool parseMapsLine(const char *line, const std::string &moduleName,
                   MemoryRegion &region) {
  if (std::strstr(line, moduleName.c_str()) == nullptr) return false;

  uintptr_t start = 0;
  uintptr_t end = 0;
  char perms[5] = {};
  if (std::sscanf(line, "%" SCNxPTR "-%" SCNxPTR " %4s", &start, &end,
                  perms) != 3) {
    return false;
  }
  if (end <= start || perms[0] != 'r') return false;
  region = MemoryRegion{start, end};
  return true;
}

void addReadableRegion(ModuleInfo &module, uintptr_t start, uintptr_t end) {
  if (!module.regions.empty() && module.regions.back().end == start) {
    module.regions.back().end = end;
    return;
  }
  module.regions.push_back(MemoryRegion{start, end});
}

bool getModuleInfo(const std::string &name, ModuleInfo &out) {
  FILE *maps = std::fopen("/proc/self/maps", "r");
  if (!maps) return false;

  char line[4096];
  while (std::fgets(line, sizeof(line), maps)) {
    MemoryRegion region{};
    if (parseMapsLine(line, name, region)) {
      addReadableRegion(out, region.start, region.end);
    }
  }
  std::fclose(maps);
  if (out.regions.empty()) return false;

  out.handle = dlopen(name.c_str(), RTLD_LAZY | RTLD_NOLOAD);
  if (!out.handle) out.handle = dlopen(name.c_str(), RTLD_LAZY);
  return true;
}

ModuleInfo getCachedModuleInfo(const std::string &moduleName) {
  {
    std::shared_lock lock(cacheMutex);
    const auto it = moduleCache.find(moduleName);
    if (it != moduleCache.end()) return it->second;
  }

  ModuleInfo module;
  if (!getModuleInfo(moduleName, module)) return {};

  std::unique_lock lock(cacheMutex);
  const auto [it, inserted] = moduleCache.emplace(moduleName, module);
  return inserted ? module : it->second;
}

ParsedPattern getCachedPattern(const std::string &signature) {
  {
    std::shared_lock lock(cacheMutex);
    const auto it = patternCache.find(signature);
    if (it != patternCache.end()) return it->second;
  }

  ParsedPattern pattern = parsePattern(signature);
  std::unique_lock lock(cacheMutex);
  const auto [it, inserted] = patternCache.emplace(signature, pattern);
  return inserted ? pattern : it->second;
}

bool matchesPatternAt(const uint8_t *data, const ParsedPattern &pattern) {
  for (const size_t index : pattern.checkIndices) {
    if (index >= pattern.anchorIndex &&
        index < pattern.anchorIndex + pattern.anchorSize) {
      continue;
    }
    if (!matches(pattern.bytes[index], data[index])) return false;
  }
  return true;
}

bool matchesAnchorAt(const uint8_t *data, size_t regionSize,
                     size_t anchorOffset, const ParsedPattern &pattern) {
  if (anchorOffset + pattern.anchorSize > regionSize) return false;
  for (size_t i = 0; i < pattern.anchorSize; ++i) {
    if (!matches(pattern.bytes[pattern.anchorIndex + i],
                 data[anchorOffset + i])) {
      return false;
    }
  }
  return true;
}

std::vector<CompiledPattern>
compilePatterns(const std::vector<std::string> &signatures,
                std::unordered_map<std::string, uintptr_t> &results) {
  std::vector<CompiledPattern> compiled;
  compiled.reserve(signatures.size());
  for (const auto &signature : signatures) {
    auto pattern = getCachedPattern(signature);
    if (pattern.bytes.empty()) {
      preloaderLogger.warn("invalid signature pattern: {}", signature);
      results[signature] = 0;
      continue;
    }
    selectAnchor(pattern);
    compiled.push_back(CompiledPattern{signature, std::move(pattern)});
  }
  return compiled;
}

uintptr_t scanPatternWindow(uintptr_t start, uintptr_t end,
                             const ParsedPattern &pattern) {
  if (end <= start || end - start < pattern.bytes.size()) return 0;
  const auto *data = reinterpret_cast<const uint8_t *>(start);
  const size_t size = end - start;
  const size_t last = size - pattern.bytes.size() + pattern.anchorIndex;
  size_t cursor = pattern.anchorIndex;
  const auto anchor = pattern.bytes[pattern.anchorIndex];
  while (cursor <= last) {
    if (isExactByte(anchor)) {
      const auto *found = static_cast<const uint8_t *>(
          std::memchr(data + cursor, anchor.value, last - cursor + 1));
      if (!found) return 0;
      cursor = static_cast<size_t>(found - data);
    } else if (!matches(anchor, data[cursor])) {
      ++cursor;
      continue;
    }
    const size_t candidate = cursor - pattern.anchorIndex;
    if (matchesAnchorAt(data, size, cursor, pattern) &&
        matchesPatternAt(data + candidate, pattern)) {
      return start + candidate;
    }
    ++cursor;
  }
  return 0;
}

void scanCompiledPatterns(const std::vector<MemoryRegion> &regions,
                          const std::vector<CompiledPattern> &patterns,
                          std::unordered_map<std::string, uintptr_t> &results) {
  if (patterns.empty()) return;
  std::vector<uintptr_t> found(patterns.size(), 0);
  size_t unresolved = patterns.size();
  size_t overlap = 0;
  for (size_t i = 0; i < patterns.size(); ++i) {
    overlap = std::max(overlap, patterns[i].pattern.bytes.size() - 1);
    if (!regions.empty() && patterns[i].pattern.checkIndices.empty()) {
      found[i] = regions.front().start;
      --unresolved;
    }
  }
  for (const auto &region : regions) {
    for (uintptr_t start = region.start; start < region.end && unresolved;) {
      const uintptr_t next = start + std::min<uintptr_t>(64 * 1024, region.end - start);
      const uintptr_t end = next + std::min<uintptr_t>(overlap, region.end - next);
      for (size_t i = 0; i < patterns.size(); ++i) {
        if (found[i]) continue;
        found[i] = scanPatternWindow(start, end, patterns[i].pattern);
        if (found[i]) --unresolved;
      }
      start = next;
    }
    if (!unresolved) break;
  }
  for (size_t i = 0; i < patterns.size(); ++i) {
    results[patterns[i].signature] = found[i];
  }
}

std::string makeSignatureCacheKey(std::string_view moduleName,
                                  std::string_view signature) {
  std::string key;
  key.reserve(moduleName.size() + signature.size() + 2);
  key.append(moduleName).append("::").append(signature);
  return key;
}

}

std::unordered_map<std::string, uintptr_t>
resolveSignatures(std::span<const std::string> signatures,
                  std::string_view moduleName) {
  std::unordered_map<std::string, uintptr_t> results;
  std::vector<std::string> pending;
  std::unordered_map<std::string, size_t> pendingLookup;

  if (moduleName.empty()) {
    for (const auto &signature : signatures) results[signature] = 0;
    return results;
  }

  {
    std::shared_lock lock(cacheMutex);
    for (const auto &signature : signatures) {
      const auto key = makeSignatureCacheKey(moduleName, signature);
      const auto cached = sigCache.find(key);
      if (cached != sigCache.end()) {
        results[signature] = cached->second;
      } else if (pendingLookup.emplace(signature, pending.size()).second) {
        pending.push_back(signature);
      }
    }
  }

  if (pending.empty()) return results;

  const ModuleInfo module = getCachedModuleInfo(std::string(moduleName));
  if (module.regions.empty()) {
    for (const auto &signature : pending) results[signature] = 0;
  } else {
    std::vector<std::string> patterns;
    patterns.reserve(pending.size());
    for (const auto &signature : pending) {
      if (module.handle) {
        if (void *symbol = dlsym(module.handle, signature.c_str())) {
          results[signature] = reinterpret_cast<uintptr_t>(symbol);
          continue;
        }
      }
      patterns.push_back(signature);
    }

    if (!patterns.empty()) {
      const auto compiled = compilePatterns(patterns, results);
      scanCompiledPatterns(module.regions, compiled, results);
    }
  }

  std::unique_lock lock(cacheMutex);
  for (const auto &signature : pending) {
    sigCache[makeSignatureCacheKey(moduleName, signature)] = results[signature];
  }
  return results;
}

uintptr_t resolveSignature(std::string_view signature,
                           std::string_view moduleName) {
  std::vector<std::string> signatures{std::string(signature)};
  const auto results = resolveSignatures(signatures, moduleName);
  const auto it = results.find(std::string(signature));
  return it == results.end() ? 0 : it->second;
}

}
