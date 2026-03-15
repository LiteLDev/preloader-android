#pragma once

#include "pl/Hook.h"
#include "pl/Signature.h"
#include "pl/api/Macro.h"
#include "pl/api/memory/Memory.h"

#include <initializer_list>
#include <type_traits>

namespace memory {

template <typename T> struct IsConstMemberFun : std::false_type {};
template <typename T, typename Ret, typename... Args>
struct IsConstMemberFun<Ret (T::*)(Args...) const> : std::true_type {};
template <typename T>
inline constexpr bool IsConstMemberFunV = IsConstMemberFun<T>::value;

template <typename T> struct AddConstAtMemberFun {
  using type = T;
};
template <typename T, typename Ret, typename... Args>
struct AddConstAtMemberFun<Ret (T::*)(Args...)> {
  using type = Ret (T::*)(Args...) const;
};
template <typename T>
using AddConstAtMemberFunT = typename AddConstAtMemberFun<T>::type;

template <typename T, typename U>
using AddConstAtMemberFunIfOriginIs =
    std::conditional_t<IsConstMemberFunV<U>, AddConstAtMemberFunT<T>, T>;

enum class HookPriority : int {
  Highest = pl::hook::PriorityHighest,
  High = pl::hook::PriorityHigh,
  Normal = pl::hook::PriorityNormal,
  Low = pl::hook::PriorityLow,
  Lowest = pl::hook::PriorityLowest,
};

inline FuncPtr resolveIdentifier(char const *identifier,
                                 char const *moduleName) {
  return reinterpret_cast<FuncPtr>(
      pl::signature::pl_resolve_signature(identifier, moduleName));
}

inline FuncPtr resolveIdentifier(
    std::initializer_list<const char *> identifiers,
    char const *moduleName) {
  for (const auto &identifier : identifiers) {
    FuncPtr result = resolveIdentifier(identifier, moduleName);
    if (result != nullptr) {
      return result;
    }
  }
  return nullptr;
}

template <typename T>
concept FuncPtrType = std::is_function_v<std::remove_pointer_t<T>> ||
                      std::is_member_function_pointer_v<T>;

template <typename T>
  requires(FuncPtrType<T> || std::is_same_v<T, uintptr_t>)
constexpr FuncPtr resolveIdentifier(T identifier, const char *moduleName) {
  (void) moduleName;
  return toFuncPtr(identifier);
}

template <typename T>
constexpr FuncPtr resolveIdentifier(char const *identifier,
                                    const char *moduleName) {
  return resolveIdentifier(identifier, moduleName);
}

template <typename T>
constexpr FuncPtr resolveIdentifier(uintptr_t address, const char *moduleName) {
  return resolveIdentifier(address, moduleName);
}

template <typename T>
constexpr FuncPtr resolveIdentifier(FuncPtr address,
                                    const char * /*moduleName*/) {
  return address;
}

template <typename T>
constexpr FuncPtr
resolveIdentifier(std::initializer_list<const char *> identifiers,
                  const char *moduleName) {
  return resolveIdentifier(identifiers, moduleName);
}

template <typename T> struct HookAutoRegister {
  HookAutoRegister() { T::hook(); }
  ~HookAutoRegister() { T::unhook(); }
  static int hook() { return T::hook(); }
  static bool unhook() { return T::unhook(); }
};

} // namespace memory

#define HOOK_IMPL(REGISTER, FUNC_PTR, STATIC, CALL, DEF_TYPE, TYPE, PRIORITY,  \
                  IDENTIFIER, MODULE, RET_TYPE, ...)                           \
  struct DEF_TYPE TYPE {                                                       \
    using FuncPtr = ::memory::FuncPtr;                                         \
    using HookPriority = ::memory::HookPriority;                               \
    using OriginFuncType = ::memory::AddConstAtMemberFunIfOriginIs<            \
        RET_TYPE FUNC_PTR(__VA_ARGS__), decltype(IDENTIFIER)>;                 \
                                                                               \
    inline static FuncPtr target{};                                            \
    inline static OriginFuncType originFunc{};                                 \
                                                                               \
    template <typename... Args> STATIC RET_TYPE origin(Args &&...params) {     \
      return CALL(std::forward<Args>(params)...);                              \
    }                                                                          \
                                                                               \
    STATIC RET_TYPE detour(__VA_ARGS__);                                       \
                                                                               \
    static int hook() {                                                        \
      target = memory::resolveIdentifier<OriginFuncType>(IDENTIFIER, MODULE);  \
      if (target == nullptr) {                                                 \
        return -1;                                                             \
      }                                                                        \
      return pl::hook::pl_hook(                                                \
          target,                                                              \
          memory::toFuncPtr(&DEF_TYPE::detour),                                \
          reinterpret_cast<FuncPtr *>(&originFunc),                            \
          static_cast<pl::hook::Priority>(PRIORITY));                          \
    }                                                                          \
                                                                               \
    static bool unhook() {                                                     \
      return pl::hook::pl_unhook(                                              \
          target, memory::toFuncPtr(&DEF_TYPE::detour));                       \
    }                                                                          \
  };                                                                           \
  REGISTER;                                                                    \
  RET_TYPE DEF_TYPE::detour(__VA_ARGS__)

#define LL_AUTO_REG_HOOK_IMPL(FUNC_PTR, STATIC, CALL, DEF_TYPE, ...)          \
  VA_EXPAND(HOOK_IMPL(                                                         \
      inline memory::HookAutoRegister<DEF_TYPE> DEF_TYPE##AutoRegister,        \
      FUNC_PTR, STATIC, CALL, DEF_TYPE, __VA_ARGS__))

#define LL_MANUAL_REG_HOOK_IMPL(...) VA_EXPAND(HOOK_IMPL(, __VA_ARGS__))

#define LL_STATIC_HOOK_IMPL(...)                                              \
  VA_EXPAND(LL_MANUAL_REG_HOOK_IMPL((*), static, originFunc, __VA_ARGS__))

#define LL_AUTO_STATIC_HOOK_IMPL(...)                                         \
  VA_EXPAND(LL_AUTO_REG_HOOK_IMPL((*), static, originFunc, __VA_ARGS__))

#define LL_INSTANCE_HOOK_IMPL(DEF_TYPE, ...)                                  \
  VA_EXPAND(LL_MANUAL_REG_HOOK_IMPL((DEF_TYPE::*), , (this->*originFunc),     \
                                    DEF_TYPE, __VA_ARGS__))

#define LL_AUTO_INSTANCE_HOOK_IMPL(DEF_TYPE, ...)                             \
  VA_EXPAND(LL_AUTO_REG_HOOK_IMPL((DEF_TYPE::*), , (this->*originFunc),       \
                                  DEF_TYPE, __VA_ARGS__))

#define LL_TYPED_STATIC_HOOK(DefType, type, priority, identifier, module,     \
                             Ret, ...)                                         \
  VA_EXPAND(LL_STATIC_HOOK_IMPL(DefType, : public type, priority, identifier, \
                                module, Ret, __VA_ARGS__))

#define LL_STATIC_HOOK(DefType, priority, identifier, module, Ret, ...)       \
  VA_EXPAND(LL_STATIC_HOOK_IMPL(DefType, , priority, identifier, module, Ret, \
                                __VA_ARGS__))

#define LL_AUTO_TYPED_STATIC_HOOK(DefType, type, priority, identifier,        \
                                  module, Ret, ...)                            \
  VA_EXPAND(LL_AUTO_STATIC_HOOK_IMPL(DefType, : public type, priority,        \
                                     identifier, module, Ret, __VA_ARGS__))

#define LL_AUTO_STATIC_HOOK(DefType, priority, identifier, module, Ret, ...)  \
  VA_EXPAND(LL_AUTO_STATIC_HOOK_IMPL(DefType, , priority, identifier, module, \
                                     Ret, __VA_ARGS__))

#define LL_TYPED_HOOK(DEF_TYPE, PRIORITY, TYPE, IDENTIFIER, MODULE, RET_TYPE, \
                      ...)                                                     \
  VA_EXPAND(LL_INSTANCE_HOOK_IMPL(DEF_TYPE, : public TYPE, PRIORITY,          \
                                  IDENTIFIER, MODULE, RET_TYPE, __VA_ARGS__))

#define LL_INSTANCE_HOOK(DEF_TYPE, PRIORITY, IDENTIFIER, MODULE, RET_TYPE,    \
                         ...)                                                  \
  VA_EXPAND(LL_INSTANCE_HOOK_IMPL(DEF_TYPE, , PRIORITY, IDENTIFIER, MODULE,   \
                                  RET_TYPE, __VA_ARGS__))

#define LL_AUTO_TYPED_INSTANCE_HOOK(DEF_TYPE, PRIORITY, TYPE, IDENTIFIER,     \
                                    MODULE, RET_TYPE, ...)                     \
  VA_EXPAND(LL_AUTO_INSTANCE_HOOK_IMPL(DEF_TYPE, : public TYPE, PRIORITY,     \
                                       IDENTIFIER, MODULE, RET_TYPE,          \
                                       __VA_ARGS__))

#define LL_AUTO_INSTANCE_HOOK(DEF_TYPE, PRIORITY, IDENTIFIER, MODULE,         \
                              RET_TYPE, ...)                                   \
  VA_EXPAND(LL_AUTO_INSTANCE_HOOK_IMPL(DEF_TYPE, , PRIORITY, IDENTIFIER,      \
                                       MODULE, RET_TYPE, __VA_ARGS__))
