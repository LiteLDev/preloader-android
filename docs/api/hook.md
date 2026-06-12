# Hook API

## Purpose

Hook API replaces a target function with a detour function and stores the original or next function pointer. Multiple detours on the same target are chained by priority.

## Headers

C:

```c
#include <pl/c/Hook.h>
```

C++:

```cpp
#include <pl/cpp/Hook.hpp>
```

## Signatures

```c
typedef void *PLFuncPtr;
typedef PLFuncPtr FuncPtr;

typedef enum PLHookPriority {
  PL_HOOK_PRIORITY_HIGHEST = 0,
  PL_HOOK_PRIORITY_HIGH = 100,
  PL_HOOK_PRIORITY_NORMAL = 200,
  PL_HOOK_PRIORITY_LOW = 300,
  PL_HOOK_PRIORITY_LOWEST = 400,
} PLHookPriority;

PLAPI int pl_hook(PLFuncPtr target, PLFuncPtr detour,
                  PLFuncPtr *originalFunc,
                  PLHookPriority priority);

PLAPI bool pl_unhook(PLFuncPtr target, PLFuncPtr detour);
```

C++ wrappers:

```cpp
namespace pl::hook {
using FuncPtr = PLFuncPtr;

enum Priority : int {
  PriorityHighest = PL_HOOK_PRIORITY_HIGHEST,
  PriorityHigh = PL_HOOK_PRIORITY_HIGH,
  PriorityNormal = PL_HOOK_PRIORITY_NORMAL,
  PriorityLow = PL_HOOK_PRIORITY_LOW,
  PriorityLowest = PL_HOOK_PRIORITY_LOWEST,
};

int pl_hook(FuncPtr target, FuncPtr detour, FuncPtr *originalFunc,
            Priority priority);
bool pl_unhook(FuncPtr target, FuncPtr detour);
int hook(FuncPtr target, FuncPtr detour, FuncPtr *originalFunc,
         Priority priority = PriorityNormal);
bool unhook(FuncPtr target, FuncPtr detour);
}
```

## pl_hook

### Purpose

Installs a hook. Adding another hook to the same target rebuilds the detour chain.

### Parameters

| Parameter | Description |
| --- | --- |
| `target` | Target function address; must not be `NULL` |
| `detour` | Replacement function address; must not be `NULL` |
| `originalFunc` | Receives the original or next function pointer; must not be `NULL` |
| `priority` | Hook priority; lower values run earlier |

### Return Value

| Value | Description |
| --- | --- |
| `0` | Success |
| `-1` | Invalid argument or installation failure |

### Example

```cpp
#include <pl/cpp/Hook.hpp>

using UpdateFn = void (*)(void *);
static UpdateFn old_update = nullptr;

static void my_update(void *self) {
  old_update(self);
}

void install(void *target) {
  pl::hook::hook(target,
                 reinterpret_cast<void *>(my_update),
                 reinterpret_cast<void **>(&old_update),
                 pl::hook::PriorityNormal);
}
```

## pl_unhook

### Purpose

Removes one detour from a target function.

### Parameters

| Parameter | Description |
| --- | --- |
| `target` | Target function address |
| `detour` | Detour function address to remove |

### Return Value

Returns `true` when removed, otherwise `false`.

## Chain Behavior

- Lower priority values run earlier.
- Same priority keeps registration order.
- `originalFunc` points to the next function in the chain; the last one points to the real original.

## Common Mistakes

- Passing `NULL` as `originalFunc`: `pl_hook` returns `-1`.
- Detour signature does not match target ABI.
- Calling the target address directly inside detour, causing recursion.
- Installing too early before the target library is loaded.
