#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace memory {

using FuncPtr = void *;

template <typename T>
  requires(sizeof(T) == sizeof(FuncPtr))
constexpr FuncPtr toFuncPtr(T value) {
  union {
    FuncPtr fp;
    T value;
  } storage{};

  storage.value = value;
  return storage.fp;
}

template <typename T>
  requires(std::is_member_function_pointer_v<T> &&
           sizeof(T) == sizeof(FuncPtr) + sizeof(ptrdiff_t))
constexpr FuncPtr toFuncPtr(T value) {
  union {
    struct {
      FuncPtr fp;
      ptrdiff_t offset;
    };
    T value;
  } storage{};

  storage.value = value;
  return storage.fp;
}

} // namespace memory
