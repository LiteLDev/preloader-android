# GlossHook 门面

## 作用

`pl/Gloss.h` 暴露底层 GlossHook 能力，包括动态库查询、内存读写、inline hook、PLT hook 和指令生成。preloader 的 Hook API 和 Patch API 都建立在这些能力之上。

对于普通 mod，优先使用：

- [Hook API](hook.md)
- [Patch API](patch.md)
- [Signature API](signature.md)

只有需要更底层控制时才直接 include `pl/Gloss.h`。

## 头文件

```cpp
#include <pl/Gloss.h>
```

## 推荐直接使用的能力

### GlossInit

```c
GLOSS_API void GlossInit(bool is_init_linker);
```

作用：初始化 GlossHook。`pl_hook` 会在第一次调用时自动初始化，通常不需要手动调用。

### GlossHook

```c
GLOSS_API GHook GlossHook(void *addr, void *new_func, void **old_func);
```

作用：对函数入口做 inline hook。更推荐使用 `pl_hook`，因为它提供 hook 链和优先级。

### GlossHookDelete

```c
GLOSS_API void GlossHookDelete(GHook hook);
```

作用：删除 GlossHook handle。通过 `pl_hook` 安装的 hook 不应手动删除底层 handle。

### WriteMemory / ReadMemory

```c
GLOSS_API void WriteMemory(void *addr, void *data, size_t size, bool vp);
GLOSS_API void *ReadMemory(void *addr, void *data, size_t size, bool vp);
```

作用：直接读写内存。更推荐使用 `pl::patch`，因为它记录原字节并支持回滚。

## 注意事项

- `Gloss.h` 是外部库门面，命名和行为跟随 GlossHook。
- 不要混用 `pl_hook` 管理的 hook 和手动 `GlossHookDelete`。
- 直接写内存前必须确认目标 ABI、指令长度和页权限。
- 底层接口更灵活，也更容易崩溃；普通 mod 不应优先使用。

