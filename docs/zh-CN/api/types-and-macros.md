# Types 与宏

## 作用

类型和宏头定义导出符号、C ABI 声明方式和基础类型别名。

## 头文件

C:

```c
#include <pl/c/Macro.h>
#include <pl/c/Types.h>
```

C++:

```cpp
#include <pl/cpp/Types.hpp>
```

旧路径：

```cpp
#include <pl/api/Macro.h>
#include <pl/api/Types.h>
```

## 宏

### VA_EXPAND

```c
#define VA_EXPAND(...) __VA_ARGS__
```

用于展开可变参数宏。hook 宏内部会使用它。

### PLAPI

```c
#ifdef PRELOADER_EXPORT
#define PLAPI __attribute__((visibility("default")))
#else
#define PLAPI
#endif
```

作用：标记需要默认可见性的导出符号。

使用场景：

```c
PLAPI void MyExportedFunction(void);
```

### PLCAPI

```c
#ifdef __cplusplus
#define PLCAPI extern "C" PLAPI
#else
#define PLCAPI extern PLAPI
#endif
```

作用：声明 C ABI 导出函数。C++ 代码中可避免 name mangling。

## 基础类型

`pl/c/Types.h` 提供以下别名：

| 类型 | 等价类型 |
| --- | --- |
| `ushort` | `unsigned short` |
| `uint` | `unsigned int` |
| `ulong` | `unsigned long` |
| `llong` | `long long` |
| `ullong` | `unsigned long long` |
| `uchar` | `unsigned char` |
| `schar` | `signed char` |
| `byte` | `uchar` |
| `ldouble` | `long double` |
| `int64` | `long long` |
| `int32` | `int` |
| `int16` | `short` |
| `int8` | `char` |
| `uint64` | `unsigned long long` |
| `uint32` | `unsigned int` |
| `uint16` | `unsigned short` |
| `uint8` | `unsigned char` |

## 注意事项

- C ABI 头只使用 C 可用类型，不暴露 STL。
- 新代码优先使用 `pl/c/*` 或 `pl/cpp/*`。
- 旧 `pl/api/*` 路径保留用于兼容已有 mod。
