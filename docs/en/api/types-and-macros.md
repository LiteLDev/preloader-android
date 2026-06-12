# Types and Macros

## Purpose

Type and macro headers define exported symbol visibility, C ABI declarations, and base type aliases.

## Headers

C:

```c
#include <pl/c/Macro.h>
#include <pl/c/Types.h>
```

C++:

```cpp
#include <pl/cpp/Types.hpp>
```

Legacy:

```cpp
#include <pl/api/Macro.h>
#include <pl/api/Types.h>
```

## VA_EXPAND

```c
#define VA_EXPAND(...) __VA_ARGS__
```

Expands variadic macro arguments. Hook macros use it internally.

## PLAPI

Marks exported symbols with default visibility when building preloader.

```c
PLAPI void MyExportedFunction(void);
```

## PLCAPI

Declares C ABI exported functions.

```c
PLCAPI void MyCFunction(void);
```

## Base Type Aliases

| Alias | Equivalent type |
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

## Notes

- C ABI headers do not expose STL types.
- Prefer `pl/c/*` or `pl/cpp/*` in new code.
- Legacy `pl/api/*` paths remain for compatibility.

