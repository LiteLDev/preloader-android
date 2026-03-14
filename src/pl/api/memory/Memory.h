#pragma once

#include <iostream>
#include <type_traits>
#include <cstdint>
#include <string>
#include <vector>

namespace memory {

using FuncPtr = void *;

template <typename T>
  requires(sizeof(T) == sizeof(FuncPtr))
constexpr FuncPtr toFuncPtr(T t) {
  union {
    FuncPtr fp;
    T t;
  } u{};
  u.t = t;
  return u.fp;
}

template <typename T>
  requires(std::is_member_function_pointer_v<T> &&
           sizeof(T) == sizeof(FuncPtr) + sizeof(ptrdiff_t))
constexpr FuncPtr toFuncPtr(T t) {
  union {
    struct {
      FuncPtr fp;
      ptrdiff_t offset;
    };
    T t;
  } u{};
  u.t = t;
  return u.fp;
}

inline uintptr_t resolveSignature(uintptr_t moduleBase, size_t moduleSize,
                                  const std::string &signature) {
  std::vector<uint16_t> pattern;
  std::vector<bool> mask;

  for (size_t i = 0; i < signature.size();) {
    if (signature[i] == ' ') {
      i++;
      continue;
    }

    if (signature[i] == '?') {
      pattern.push_back(0);
      mask.push_back(false);
      i += (i + 1 < signature.size() && signature[i + 1] == '?') ? 2 : 1;
    } else {
      if (i + 1 >= signature.size()) {
        return 0;
      }

      char buf[3] = {signature[i], signature[i + 1], 0};
      char *end;
      unsigned long value = strtoul(buf, &end, 16);
      if (end != buf + 2) {
        return 0;
      }

      pattern.push_back(static_cast<uint16_t>(value));
      mask.push_back(true);
      i += 2;
    }
  }

  if (pattern.empty()) {
    return moduleBase;
  }

  const uint8_t *start = reinterpret_cast<const uint8_t *>(moduleBase);
  const uint8_t *end = start + moduleSize - pattern.size();

  for (const uint8_t *ptr = start; ptr <= end; ptr++) {
    bool found = true;

    for (size_t i = 0; i < pattern.size(); i++) {
      if (mask[i] && ptr[i] != pattern[i]) {
        found = false;
        break;
      }
    }

    if (found) {
      return reinterpret_cast<uintptr_t>(ptr);
    }
  }

  return 0;
}

} // namespace memory

