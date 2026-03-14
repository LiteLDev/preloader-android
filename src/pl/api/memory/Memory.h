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

} // namespace memory

