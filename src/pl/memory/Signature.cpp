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

using ByteFrequencyTable = std::array<size_t, 256>;

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
std::unordered_map<std::string, ByteFrequencyTable> frequencyCache;
std::shared_mutex cacheMutex;

int hexValue(char ch) {
  if (ch >= '0' && ch <= '9') {
    return ch - '0';
  }
  if (ch >= 'a' && ch <= 'f') {
    return ch - 'a' + 10;
  }
  if (ch >= 'A' && ch <= 'F') {
    return ch - 'A' + 10;
  }
  return -1;
}

bool parsePatternToken(std::string_view token, PatternByte &byte) {
  if (token == "?" || token == "??") {
    byte = PatternByte{0, 0};
    return true;
  }

  if (token.size() != 2) {
    return false;
  }

  uint8_t value = 0;
  uint8_t mask = 0;
  for (size_t i = 0; i < token.size(); ++i) {
    const char ch = token[i];
    const auto shift = static_cast<uint8_t>((1 - i) * 4);
    if (ch == '?') {
      continue;
    }

    const int digit = hexValue(ch);
    if (digit < 0) {
      return false;
    }

    value |= static_cast<uint8_t>(digit << shift);
    mask |= static_cast<uint8_t>(0xF << shift);
  }

  byte = PatternByte{value, mask};
  return true;
}

void appendPatternByte(ParsedPattern &pattern, PatternByte byte) {
  const size_t byteIndex = pattern.bytes.size();
  if (byte.mask != 0) {
    if (pattern.checkIndices.empty()) {
      pattern.anchorIndex = byteIndex;
    }
    pattern.checkIndices.push_back(byteIndex);
  }
  pattern.bytes.push_back(byte);
}

bool appendPatternToken(std::string_view token, ParsedPattern &pattern) {
  PatternByte byte{};
  if (parsePatternToken(token, byte)) {
    appendPatternByte(pattern, byte);
    return true;
  }

  if (token.empty() || token.size() % 2 != 0) {
    return false;
  }

  std::vector<PatternByte> bytes;
  bytes.reserve(token.size() / 2);
  for (size_t pos = 0; pos < token.size(); pos += 2) {
    if (!parsePatternToken(token.substr(pos, 2), byte)) {
      return false;
    }
    bytes.push_back(byte);
  }

  for (const PatternByte parsedByte : bytes) {
    appendPatternByte(pattern, parsedByte);
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
    if (pos >= signature.size()) {
      break;
    }

    const size_t tokenStart = pos;
    while (pos < signature.size() &&
           !std::isspace(static_cast<unsigned char>(signature[pos]))) {
      ++pos;
    }

    if (!appendPatternToken(signature.substr(tokenStart, pos - tokenStart),
                            pattern)) {
      pattern.bytes.clear();
      pattern.checkIndices.clear();
      return pattern;
    }
  }

  return pattern;
}

bool matches(PatternByte pattern, uint8_t value) {
  return (value & pattern.mask) == pattern.value;
}

bool isExactByte(PatternByte byte) { return byte.mask == 0xFF; }

int countMaskBits(uint8_t mask) {
  int count = 0;
  while (mask != 0) {
    mask &= static_cast<uint8_t>(mask - 1);
    ++count;
  }
  return count;
}

size_t matchingByteFrequency(PatternByte byte,
                             const ByteFrequencyTable &frequencies) {
  size_t frequency = 0;
  for (size_t value = 0; value < frequencies.size(); ++value) {
    if (matches(byte, static_cast<uint8_t>(value))) {
      frequency += frequencies[value];
    }
  }
  return frequency;
}

bool isBetterAnchor(PatternByte candidate, size_t candidateIndex,
                    PatternByte current, size_t currentIndex,
                    const ByteFrequencyTable &frequencies) {
  const size_t candidateFrequency =
      matchingByteFrequency(candidate, frequencies);
  const size_t currentFrequency = matchingByteFrequency(current, frequencies);

  if (candidateFrequency != currentFrequency) {
    return candidateFrequency < currentFrequency;
  }

  const int candidateMaskBits = countMaskBits(candidate.mask);
  const int currentMaskBits = countMaskBits(current.mask);
  if (candidateMaskBits != currentMaskBits) {
    return candidateMaskBits > currentMaskBits;
  }

  return candidateIndex > currentIndex;
}

size_t exactAnchorCost(const ParsedPattern &pattern, size_t start, size_t size,
                       const ByteFrequencyTable &frequencies) {
  size_t cost = 0;
  for (size_t i = 0; i < size; ++i) {
    cost += frequencies[pattern.bytes[start + i].value];
  }
  return cost;
}

bool isBetterExactAnchor(const ParsedPattern &pattern, size_t candidateIndex,
                         size_t candidateSize, size_t currentIndex,
                         size_t currentSize,
                         const ByteFrequencyTable &frequencies) {
  const size_t candidateHeadFrequency =
      frequencies[pattern.bytes[candidateIndex].value];
  const size_t currentHeadFrequency =
      frequencies[pattern.bytes[currentIndex].value];
  if (candidateHeadFrequency != currentHeadFrequency) {
    return candidateHeadFrequency < currentHeadFrequency;
  }

  if (candidateSize != currentSize) {
    return candidateSize > currentSize;
  }

  const size_t candidateCost =
      exactAnchorCost(pattern, candidateIndex, candidateSize, frequencies);
  const size_t currentCost =
      exactAnchorCost(pattern, currentIndex, currentSize, frequencies);
  if (candidateCost != currentCost) {
    return candidateCost < currentCost;
  }

  return candidateIndex > currentIndex;
}

void selectAnchor(ParsedPattern &pattern,
                  const ByteFrequencyTable &frequencies) {
  if (pattern.checkIndices.empty()) {
    return;
  }

  bool hasExactAnchor = false;
  size_t bestExactIndex = 0;
  size_t bestExactSize = 0;

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

    const size_t runSize = runEnd - runStart;
    const size_t maxAnchorSize = std::min(runSize, kMaxExactAnchorSize);
    for (size_t size = 1; size <= maxAnchorSize; ++size) {
      for (size_t start = runStart; start + size <= runEnd; ++start) {
        if (!hasExactAnchor ||
            isBetterExactAnchor(pattern, start, size, bestExactIndex,
                                bestExactSize, frequencies)) {
          hasExactAnchor = true;
          bestExactIndex = start;
          bestExactSize = size;
        }
      }
    }

    runStart = runEnd;
  }

  if (hasExactAnchor) {
    pattern.anchorIndex = bestExactIndex;
    pattern.anchorSize = bestExactSize;
    return;
  }

  size_t bestIndex = pattern.checkIndices.front();
  for (const size_t index : pattern.checkIndices) {
    if (isBetterAnchor(pattern.bytes[index], index, pattern.bytes[bestIndex],
                       bestIndex, frequencies)) {
      bestIndex = index;
    }
  }

  pattern.anchorIndex = bestIndex;
  pattern.anchorSize = 1;
}

bool parseMapsLine(const char *line, const std::string &moduleName,
                   MemoryRegion &region) {
  if (std::strstr(line, moduleName.c_str()) == nullptr) {
    return false;
  }

  uintptr_t start = 0;
  uintptr_t end = 0;
  char perms[5] = {};
  if (std::sscanf(line, "%" SCNxPTR "-%" SCNxPTR " %4s", &start, &end,
                  perms) != 3) {
    return false;
  }
  if (end <= start || perms[0] != 'r') {
    return false;
  }

  region = MemoryRegion{start, end};
  return true;
}

void addReadableRegion(ModuleInfo &module, uintptr_t start, uintptr_t end) {
  if (!module.regions.empty()) {
    MemoryRegion &last = module.regions.back();
    if (last.end == start) {
      last.end = end;
      return;
    }
  }

  module.regions.push_back(MemoryRegion{start, end});
}

bool getModuleInfo(const std::string &name, ModuleInfo &out) {
  FILE *maps = std::fopen("/proc/self/maps", "r");
  if (!maps) {
    return false;
  }

  char line[4096];
  while (std::fgets(line, sizeof(line), maps)) {
    MemoryRegion region{};
    if (parseMapsLine(line, name, region)) {
      addReadableRegion(out, region.start, region.end);
    }
  }
  std::fclose(maps);

  if (out.regions.empty()) {
    return false;
  }

  out.handle = dlopen(name.c_str(), RTLD_LAZY | RTLD_NOLOAD);
  if (!out.handle) {
    out.handle = dlopen(name.c_str(), RTLD_LAZY);
  }

  return true;
}

ModuleInfo getCachedModuleInfo(const std::string &moduleName) {
  {
    std::shared_lock lk(cacheMutex);
    if (auto it = moduleCache.find(moduleName); it != moduleCache.end()) {
      return it->second;
    }
  }

  ModuleInfo mod;
  if (!getModuleInfo(moduleName, mod)) {
    return {};
  }

  {
    std::unique_lock lk(cacheMutex);
    if (auto it = moduleCache.find(moduleName); it != moduleCache.end()) {
      return it->second;
    }
    moduleCache[moduleName] = mod;
  }

  return mod;
}

ParsedPattern getCachedPattern(const std::string &signature) {
  {
    std::shared_lock lk(cacheMutex);
    if (auto it = patternCache.find(signature); it != patternCache.end()) {
      return it->second;
    }
  }

  ParsedPattern pattern = parsePattern(signature);
  {
    std::unique_lock lk(cacheMutex);
    if (auto it = patternCache.find(signature); it != patternCache.end()) {
      return it->second;
    }
    patternCache[signature] = pattern;
  }

  return pattern;
}

ByteFrequencyTable buildByteFrequencyTable(
    const std::vector<MemoryRegion> &regions) {
  ByteFrequencyTable frequencies{};

  for (const auto &region : regions) {
    const auto *data = reinterpret_cast<const uint8_t *>(region.start);
    const size_t regionSize = region.end - region.start;
    for (size_t offset = 0; offset < regionSize; ++offset) {
      ++frequencies[data[offset]];
    }
  }

  return frequencies;
}

ByteFrequencyTable
getCachedByteFrequencyTable(const std::string &moduleName,
                            const std::vector<MemoryRegion> &regions) {
  {
    std::shared_lock lk(cacheMutex);
    if (auto it = frequencyCache.find(moduleName); it != frequencyCache.end()) {
      return it->second;
    }
  }

  const ByteFrequencyTable frequencies = buildByteFrequencyTable(regions);
  {
    std::unique_lock lk(cacheMutex);
    if (auto it = frequencyCache.find(moduleName); it != frequencyCache.end()) {
      return it->second;
    }
    frequencyCache[moduleName] = frequencies;
  }

  return frequencies;
}

bool matchesPatternAt(const uint8_t *data, const ParsedPattern &pattern) {
  for (const size_t index : pattern.checkIndices) {
    if (index >= pattern.anchorIndex &&
        index < pattern.anchorIndex + pattern.anchorSize) {
      continue;
    }

    if (!matches(pattern.bytes[index], data[index])) {
      return false;
    }
  }

  return true;
}

bool matchesAnchorAt(const uint8_t *data, size_t regionSize,
                     size_t anchorOffset, const ParsedPattern &pattern) {
  if (anchorOffset + pattern.anchorSize > regionSize) {
    return false;
  }

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
                const ByteFrequencyTable &frequencies,
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

    selectAnchor(pattern, frequencies);
    compiled.push_back(CompiledPattern{signature, std::move(pattern)});
  }

  return compiled;
}

void scanCompiledPatterns(const std::vector<MemoryRegion> &regions,
                          const std::vector<CompiledPattern> &patterns,
                          std::unordered_map<std::string, uintptr_t> &results) {
  if (patterns.empty()) {
    return;
  }

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

  std::array<std::vector<size_t>, 256> exactAnchorBuckets;
  std::vector<size_t> maskedAnchorPatterns;
  for (size_t i = 0; i < patterns.size(); ++i) {
    if (!active[i]) {
      continue;
    }

    const auto &pattern = patterns[i].pattern;
    const PatternByte anchor = pattern.bytes[pattern.anchorIndex];
    if (isExactByte(anchor)) {
      exactAnchorBuckets[anchor.value].push_back(i);
    } else {
      maskedAnchorPatterns.push_back(i);
    }
  }

  auto tryMatch = [&](const MemoryRegion &region, const uint8_t *data,
                      size_t regionSize, size_t anchorOffset,
                      size_t patternIndex) {
    if (!active[patternIndex]) {
      return;
    }

    const auto &pattern = patterns[patternIndex].pattern;
    if (regionSize < pattern.bytes.size() ||
        anchorOffset < pattern.anchorIndex) {
      return;
    }

    if (!matchesAnchorAt(data, regionSize, anchorOffset, pattern)) {
      return;
    }

    const size_t candidateOffset = anchorOffset - pattern.anchorIndex;
    if (candidateOffset > regionSize - pattern.bytes.size()) {
      return;
    }

    if (!matchesPatternAt(data + candidateOffset, pattern)) {
      return;
    }

    found[patternIndex] = region.start + candidateOffset;
    active[patternIndex] = false;
    --unresolved;
  };

  for (const auto &region : regions) {
    if (unresolved == 0) {
      break;
    }

    const auto *data = reinterpret_cast<const uint8_t *>(region.start);
    const size_t regionSize = region.end - region.start;

    for (size_t offset = 0; offset < regionSize && unresolved != 0; ++offset) {
      const uint8_t value = data[offset];

      for (const size_t patternIndex : exactAnchorBuckets[value]) {
        tryMatch(region, data, regionSize, offset, patternIndex);
      }

      for (const size_t patternIndex : maskedAnchorPatterns) {
        const auto &pattern = patterns[patternIndex].pattern;
        const PatternByte anchor = pattern.bytes[pattern.anchorIndex];
        if (matches(anchor, value)) {
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

} // namespace

std::unordered_map<std::string, uintptr_t>
resolveSignatures(std::span<const std::string> signatures,
                  std::string_view moduleName) {
  std::unordered_map<std::string, uintptr_t> results;
  std::vector<std::string> pendingSignatures;
  std::unordered_map<std::string, size_t> pendingLookup;

  if (moduleName.empty()) {
    for (const auto &signature : signatures) {
      results[signature] = 0;
    }
    return results;
  }

  {
    std::shared_lock lk(cacheMutex);
    for (const auto &signature : signatures) {
      const auto key = makeSignatureCacheKey(moduleName, signature);
      if (const auto it = sigCache.find(key); it != sigCache.end()) {
        results[signature] = it->second;
        continue;
      }

      if (pendingLookup.find(signature) != pendingLookup.end()) {
        continue;
      }

      pendingLookup[signature] = pendingSignatures.size();
      pendingSignatures.push_back(signature);
    }
  }

  if (pendingSignatures.empty()) {
    return results;
  }

  const ModuleInfo mod = getCachedModuleInfo(std::string(moduleName));
  if (mod.regions.empty()) {
    for (const auto &signature : pendingSignatures) {
      results[signature] = 0;
    }
  } else {
    std::vector<std::string> patternSignatures;
    patternSignatures.reserve(pendingSignatures.size());

    for (const auto &signature : pendingSignatures) {
      if (mod.handle) {
        if (void *sym = dlsym(mod.handle, signature.c_str())) {
          results[signature] = reinterpret_cast<uintptr_t>(sym);
          continue;
        }
      }
      patternSignatures.push_back(signature);
    }

    if (!patternSignatures.empty()) {
      const auto frequencies =
          getCachedByteFrequencyTable(std::string(moduleName), mod.regions);
      const auto compiled =
          compilePatterns(patternSignatures, frequencies, results);
      scanCompiledPatterns(mod.regions, compiled, results);
    }
  }

  {
    std::unique_lock lk(cacheMutex);
    for (const auto &signature : pendingSignatures) {
      sigCache[makeSignatureCacheKey(moduleName, signature)] =
          results[signature];
    }
  }

  return results;
}

uintptr_t resolveSignature(std::string_view signature,
                           std::string_view moduleName) {
  std::vector<std::string> signatures{std::string(signature)};
  const auto results = resolveSignatures(signatures, moduleName);
  if (const auto it = results.find(std::string(signature)); it != results.end()) {
    return it->second;
  }
  return 0;
}

} // namespace pl::memory
