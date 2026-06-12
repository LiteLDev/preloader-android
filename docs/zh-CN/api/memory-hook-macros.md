# Memory Hook 宏

## 作用

`pl/api/memory/Hook.h` 提供 C++ hook 宏，用更少样板代码声明 detour、保存原函数并注册 hook。

## 头文件

```cpp
#include <pl/api/memory/Hook.h>
```

## 相关类型

```cpp
namespace memory {
enum class HookPriority : int {
  Highest,
  High,
  Normal,
  Low,
  Lowest,
};
}
```

## 常用宏

| 宏 | 作用 |
| --- | --- |
| `LL_STATIC_HOOK` | 定义静态函数 hook，需要手动调用 `hook()` |
| `LL_AUTO_STATIC_HOOK` | 定义静态函数 hook，并自动注册 |
| `LL_INSTANCE_HOOK` | 定义成员函数 hook，需要手动调用 `hook()` |
| `LL_AUTO_INSTANCE_HOOK` | 定义成员函数 hook，并自动注册 |
| `LL_TYPED_STATIC_HOOK` | 静态 hook，并继承一个自定义类型 |
| `LL_AUTO_TYPED_STATIC_HOOK` | 自动注册的 typed 静态 hook |
| `LL_TYPED_HOOK` | 成员 hook，并继承一个自定义类型 |
| `LL_AUTO_TYPED_INSTANCE_HOOK` | 自动注册的 typed 成员 hook |

## 参数含义

以 `LL_STATIC_HOOK` 为例：

```cpp
LL_STATIC_HOOK(DefType, priority, identifier, module, Ret, ...)
```

| 参数 | 说明 |
| --- | --- |
| `DefType` | 生成的 hook 类型名 |
| `priority` | `memory::HookPriority` |
| `identifier` | 目标函数地址、函数指针、符号名或 pattern |
| `module` | 解析符号名或 pattern 时使用的模块名 |
| `Ret` | 返回值类型 |
| `...` | 目标函数参数列表 |

## 静态函数示例

```cpp
#include <pl/api/memory/Hook.h>

LL_STATIC_HOOK(MyTickHook,
               memory::HookPriority::Normal,
               "Game_tick",
               "libminecraftpe.so",
               void,
               void *self) {
  origin(self);
}

void install() {
  MyTickHook::hook();
}

void uninstall() {
  MyTickHook::unhook();
}
```

## 自动注册示例

```cpp
#include <pl/api/memory/Hook.h>

LL_AUTO_STATIC_HOOK(MyAutoHook,
                    memory::HookPriority::High,
                    "Game_tick",
                    "libminecraftpe.so",
                    void,
                    void *self) {
  origin(self);
}
```

## 直接地址示例

```cpp
LL_STATIC_HOOK(MyAddressHook,
               memory::HookPriority::Normal,
               reinterpret_cast<uintptr_t>(target_address),
               nullptr,
               int,
               int value) {
  return origin(value);
}
```

## 注意事项

- `identifier` 是字符串时会通过 Signature API 解析；解析失败时 `hook()` 返回 `-1`。
- `origin(...)` 会调用原函数或 hook 链中的下一个函数。
- detour 参数和返回值必须与目标函数 ABI 匹配。
- 自动注册发生在静态初始化阶段，目标库未加载时可能失败；这种情况应使用手动注册宏。
