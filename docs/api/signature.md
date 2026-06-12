# Signature API

## Purpose

Signature API resolves a symbol name or byte pattern inside a loaded module and returns the matched address.

## Headers

C:

```c
#include <pl/c/Signature.h>
```

C++:

```cpp
#include <pl/cpp/Signature.hpp>
```

## Signatures

```c
PLAPI uintptr_t pl_resolve_signature(const char *signature,
                                     const char *moduleName);
```

C++:

```cpp
namespace pl::signature {
uintptr_t resolveSignature(const std::string &signature,
                           const std::string &moduleName);
std::unordered_map<std::string, uintptr_t>
resolveSignatures(const std::vector<std::string> &signatures,
                  const std::string &moduleName);
}
```

## pl_resolve_signature

### Purpose

First tries to resolve `signature` as a symbol name. If not found, treats it as a byte pattern.

### Parameters

| Parameter | Description |
| --- | --- |
| `signature` | Symbol name or byte pattern; must not be `NULL` |
| `moduleName` | Module name or path fragment; must not be `NULL` |

### Return Value

Matched address. Returns `0` on failure.

## Pattern Format

```text
48 8B ?? ?? 89
488B????89
```

Wildcards:

| Pattern | Description |
| --- | --- |
| `?` | Whole byte wildcard |
| `??` | Whole byte wildcard |
| `A?` | Low nibble wildcard |
| `?F` | High nibble wildcard |

## C Example

```c
#include <pl/c/Signature.h>

uintptr_t addr = pl_resolve_signature("SomeSymbol", "libminecraftpe.so");
if (addr == 0) {
  addr = pl_resolve_signature("48 8B ?? ?? 89", "libminecraftpe.so");
}
```

## C++ Batch Example

```cpp
#include <pl/cpp/Signature.hpp>

auto results = pl::signature::resolveSignatures(
    {"SymbolA", "48 8B ?? ?? 89"},
    "libminecraftpe.so");

uintptr_t symbolA = results["SymbolA"];
```

## Notes

- `moduleName` must match a module path in `/proc/self/maps`.
- Empty or invalid patterns return `0`.
- Results are cached; do not assume cached addresses survive module reloads.
- Prefer `resolveSignatures` when resolving multiple patterns.
