#include "pl/memory/Vtable.hpp"

#include <cstring>
#include <limits>
#include <mutex>
#include <string>

#include "pl/Gloss.h"

namespace pl::memory {
namespace {

std::once_flag glossInitOnce;

void ensureGlossInitialized() {
  std::call_once(glossInitOnce, [] { GlossInit(true); });
}

std::string_view normalizeTypeInfoName(std::string_view typeInfoName) {
  constexpr std::string_view kTypeInfoNamePrefix = "_ZTS";
  if (typeInfoName.starts_with(kTypeInfoNamePrefix)) {
    return typeInfoName.substr(kTypeInfoNamePrefix.size());
  }
  return typeInfoName;
}

uintptr_t findBytes(uintptr_t base, size_t size, const void *pattern,
                    size_t patternSize) {
  if (!base || !pattern || patternSize == 0 || size < patternSize) {
    return 0;
  }

  const auto *patternBytes = static_cast<const uint8_t *>(pattern);
  const uint8_t first = patternBytes[0];
  const size_t lastStart = size - patternSize;
  size_t offset = 0;

  while (offset <= lastStart) {
    const size_t remainingStarts = lastStart - offset + 1;
    const void *match = std::memchr(
        reinterpret_cast<const void *>(base + offset), first, remainingStarts);
    if (!match) {
      return 0;
    }

    const auto candidate = reinterpret_cast<uintptr_t>(match);
    const size_t candidateOffset = candidate - base;
    if (std::memcmp(match, pattern, patternSize) == 0) {
      return candidate;
    }
    offset = candidateOffset + 1;
  }

  return 0;
}

bool canReadPointer(size_t sectionSize, size_t offset) {
  return offset <= sectionSize && sectionSize - offset >= sizeof(uintptr_t);
}

uintptr_t readPointer(uintptr_t base, size_t offset) {
  uintptr_t value = 0;
  std::memcpy(&value, reinterpret_cast<const void *>(base + offset),
              sizeof(value));
  return value;
}

uintptr_t findTypeInfo(uintptr_t dataRelRo, size_t dataRelRoSize,
                       uintptr_t typeName) {
  if (dataRelRoSize < sizeof(uintptr_t) * 2) {
    return 0;
  }

  for (size_t offset = sizeof(uintptr_t); canReadPointer(dataRelRoSize, offset);
       offset += sizeof(uintptr_t)) {
    if (readPointer(dataRelRo, offset) == typeName) {
      return dataRelRo + offset - sizeof(uintptr_t);
    }
  }
  return 0;
}

bool checkedSlotOffset(size_t addressPointOffset, size_t slot,
                       size_t sectionSize, size_t &slotOffset) {
  if (slot > std::numeric_limits<size_t>::max() / sizeof(uintptr_t)) {
    return false;
  }

  const size_t slotBytes = slot * sizeof(uintptr_t);
  if (addressPointOffset > std::numeric_limits<size_t>::max() - slotBytes) {
    return false;
  }

  slotOffset = addressPointOffset + slotBytes;
  return canReadPointer(sectionSize, slotOffset);
}

uintptr_t readVtableSlot(uintptr_t dataRelRo, size_t dataRelRoSize,
                         size_t typeInfoPointerOffset, size_t slot) {
  const size_t addressPointOffset = typeInfoPointerOffset + sizeof(uintptr_t);
  size_t slotOffset = 0;
  if (!checkedSlotOffset(addressPointOffset, slot, dataRelRoSize, slotOffset)) {
    return 0;
  }
  return readPointer(dataRelRo, slotOffset);
}

uintptr_t findPrimaryVtableSlot(uintptr_t dataRelRo, size_t dataRelRoSize,
                                uintptr_t typeInfo, size_t slot) {
  if (dataRelRoSize < sizeof(uintptr_t) * 3) {
    return 0;
  }

  for (size_t offset = sizeof(uintptr_t); canReadPointer(dataRelRoSize, offset);
       offset += sizeof(uintptr_t)) {
    if (readPointer(dataRelRo, offset) != typeInfo) {
      continue;
    }

    const uintptr_t offsetToTop =
        readPointer(dataRelRo, offset - sizeof(uintptr_t));
    if (offsetToTop != 0) {
      continue;
    }

    if (const uintptr_t slotValue =
            readVtableSlot(dataRelRo, dataRelRoSize, offset, slot)) {
      return slotValue;
    }
  }

  return 0;
}

} // namespace

uintptr_t resolveVtableFunction(std::string_view typeInfoName, size_t slot,
                                std::string_view moduleName) {
  if (typeInfoName.empty() || moduleName.empty()) {
    return 0;
  }

  const std::string_view normalizedName = normalizeTypeInfoName(typeInfoName);
  if (normalizedName.empty()) {
    return 0;
  }

  const std::string name(normalizedName);
  const std::string module(moduleName);

  ensureGlossInitialized();

  size_t rodataSize = 0;
  const uintptr_t rodata =
      GlossGetLibSection(module.c_str(), ".rodata", &rodataSize);
  if (!rodata || rodataSize == 0) {
    return 0;
  }

  const uintptr_t typeName =
      findBytes(rodata, rodataSize, name.c_str(), name.size() + 1);
  if (!typeName) {
    return 0;
  }

  size_t dataRelRoSize = 0;
  const uintptr_t dataRelRo =
      GlossGetLibSection(module.c_str(), ".data.rel.ro", &dataRelRoSize);
  if (!dataRelRo || dataRelRoSize == 0) {
    return 0;
  }

  const uintptr_t typeInfo = findTypeInfo(dataRelRo, dataRelRoSize, typeName);
  if (!typeInfo) {
    return 0;
  }

  return findPrimaryVtableSlot(dataRelRo, dataRelRoSize, typeInfo, slot);
}

} // namespace pl::memory
