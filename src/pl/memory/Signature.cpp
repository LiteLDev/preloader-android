#include "pl/memory/Signature.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cinttypes>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <dlfcn.h>
#include <mutex>
#include <queue>
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

struct AnchorNode {
  std::array<int, 256> next{};
  int failure = 0;
  std::vector<size_t> outputs;

  AnchorNode() { next.fill(-1); }
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

std::vector<AnchorNode>
buildAnchorAutomaton(const std::vector<CompiledPattern> &patterns,
                     const std::vector<bool> &active,
                     std::vector<size_t> &maskedPatterns) {
  std::vector<AnchorNode> nodes(1);

  for (size_t index = 0; index < patterns.size(); ++index) {
    if (!active[index]) continue;
    const auto &pattern = patterns[index].pattern;
    bool exact = true;
    for (size_t i = 0; i < pattern.anchorSize; ++i) {
      if (!isExactByte(pattern.bytes[pattern.anchorIndex + i])) {
        exact = false;
        break;
      }
    }
    if (!exact) {
      maskedPatterns.push_back(index);
      continue;
    }

    int state = 0;
    for (size_t i = 0; i < pattern.anchorSize; ++i) {
      const uint8_t value = pattern.bytes[pattern.anchorIndex + i].value;
      int next = nodes[state].next[value];
      if (next == -1) {
        next = static_cast<int>(nodes.size());
        nodes[state].next[value] = next;
        nodes.emplace_back();
      }
      state = next;
    }
    nodes[state].outputs.push_back(index);
  }

  std::queue<int> queue;
  for (size_t value = 0; value < 256; ++value) {
    int &next = nodes[0].next[value];
    if (next == -1) {
      next = 0;
    } else {
      nodes[next].failure = 0;
      queue.push(next);
    }
  }

  while (!queue.empty()) {
    const int state = queue.front();
    queue.pop();
    for (size_t value = 0; value < 256; ++value) {
      int &next = nodes[state].next[value];
      if (next == -1) {
        next = nodes[nodes[state].failure].next[value];
        continue;
      }

      const int failure = nodes[nodes[state].failure].next[value];
      nodes[next].failure = failure;
      const auto &outputs = nodes[failure].outputs;
      nodes[next].outputs.insert(nodes[next].outputs.end(), outputs.begin(),
                                 outputs.end());
      queue.push(next);
    }
  }

  return nodes;
}

void scanCompiledPatterns(const std::vector<MemoryRegion> &regions,
                          const std::vector<CompiledPattern> &patterns,
                          std::unordered_map<std::string, uintptr_t> &results) {
  if (patterns.empty()) return;

  std::vector<uintptr_t> found(patterns.size(), 0);
  std::vector<bool> active(patterns.size(), true);
  size_t unresolved = patterns.size();

  if (regions.empty()) {
    unresolved = 0;
  } else {
    for (size_t i = 0; i < patterns.size(); ++i) {
      if (patterns[i].pattern.checkIndices.empty()) {
        found[i] = regions.front().start;
        active[i] = false;
        --unresolved;
      }
    }
  }

  std::vector<size_t> maskedPatterns;
  const auto nodes = buildAnchorAutomaton(patterns, active, maskedPatterns);

  auto tryMatch = [&](const MemoryRegion &region, const uint8_t *data,
                      size_t regionSize, size_t anchorOffset,
                      size_t patternIndex) {
    if (!active[patternIndex]) return;
    const auto &pattern = patterns[patternIndex].pattern;
    if (regionSize < pattern.bytes.size() ||
        anchorOffset < pattern.anchorIndex ||
        !matchesAnchorAt(data, regionSize, anchorOffset, pattern)) {
      return;
    }

    const size_t candidateOffset = anchorOffset - pattern.anchorIndex;
    if (candidateOffset > regionSize - pattern.bytes.size() ||
        !matchesPatternAt(data + candidateOffset, pattern)) {
      return;
    }

    found[patternIndex] = region.start + candidateOffset;
    active[patternIndex] = false;
    --unresolved;
  };

  for (const auto &region : regions) {
    if (unresolved == 0) break;
    const auto *data = reinterpret_cast<const uint8_t *>(region.start);
    const size_t regionSize = region.end - region.start;
    int state = 0;

    for (size_t offset = 0; offset < regionSize && unresolved != 0; ++offset) {
      state = nodes[state].next[data[offset]];
      for (const size_t patternIndex : nodes[state].outputs) {
        const auto anchorSize = patterns[patternIndex].pattern.anchorSize;
        if (offset + 1 >= anchorSize) {
          tryMatch(region, data, regionSize, offset + 1 - anchorSize,
                   patternIndex);
        }
      }

      for (const size_t patternIndex : maskedPatterns) {
        if (!active[patternIndex]) continue;
        const auto &pattern = patterns[patternIndex].pattern;
        if (matches(pattern.bytes[pattern.anchorIndex], data[offset])) {
          tryMatch(region, data, regionSize, offset, patternIndex);
        }
      }
    }
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
