# Patch API

## Purpose

Patch API reads, writes, and reverts process memory. The C API is the stable ABI boundary; the C++ API is an inline convenience wrapper.

## Header

C:

```c
#include <pl/c/Patch.h>
```

C++:

```cpp
#include <pl/cpp/Patch.hpp>
```

## Signatures

C:

```c
bool pl_patch_write_bytes(uintptr_t addr, const uint8_t *bytes,
                          size_t len, const char *name);
bool pl_patch_write_hex(uintptr_t addr, const char *bytes,
                        const char *name);
size_t pl_patch_read_bytes(uintptr_t addr, uint8_t *out, size_t len);
bool pl_patch_revert(const char *name);
void pl_patch_revert_all(void);
```

C++:

```cpp
namespace pl::patch {
bool writeBytes(uintptr_t addr, const std::string &bytes,
                const std::string &name);
bool writeBytes(uintptr_t addr, const std::vector<uint8_t> &bytes,
                const std::string &name);
std::vector<uint8_t> readBytes(uintptr_t addr, size_t len);
bool revert(const std::string &name);
void revertAll();
}
```

## pl_patch_write_bytes / writeBytes

### Purpose

Writes bytes to an address and stores original bytes under `name`.

### Parameters

| Parameter | Description |
| --- | --- |
| `addr` | Target address |
| `bytes` | Byte buffer or hex byte string, such as `"00 00 80 D2"` |
| `len` | Number of bytes in the buffer |
| `name` | Patch name for later revert |

### Return Value

Returns `true` on success. Returns `false` when bytes are empty, a hex string is invalid, `addr` is `0`, the target range is not readable, the address range overflows, or memory permission changes fail.

### C Example

```c
#include <pl/c/Patch.h>

const uint8_t bytes[] = {0x00, 0x00, 0x80, 0xD2, 0xC0, 0x03, 0x5F, 0xD6};
bool ok = pl_patch_write_bytes(address, bytes, sizeof(bytes), "return_zero");
```

### C++ Example

```cpp
#include <pl/cpp/Patch.hpp>

bool ok = pl::patch::writeBytes(address, "00 00 80 D2 C0 03 5F D6",
                                "return_zero");
```

## pl_patch_read_bytes / readBytes

### Purpose

Reads bytes from an address.

### Parameters

| Parameter | Description |
| --- | --- |
| `addr` | Start address |
| `out` | Caller-owned output buffer |
| `len` | Number of bytes to read |

### Return Value

The C API returns the number of bytes read. It returns `0` when `out` is `NULL`, `addr` is `0`, `len` is `0`, the address range overflows, or the target range is not readable.

The C++ wrapper returns the read bytes, or an empty vector on failure.

## pl_patch_revert / revert

### Purpose

Reverts one named patch.

### Parameters

| Parameter | Description |
| --- | --- |
| `name` | Patch name passed to write |

### Return Value

Returns `true` on success, otherwise `false`.

## pl_patch_revert_all / revertAll

### Purpose

Reverts all recorded patches.

## Notes

- Reusing a patch name overwrites the previous record.
- Saved original byte length equals the write length.
- Hex strings are whitespace-separated bytes. Each byte token must contain one or two hex digits.
- `writeBytes` and `readBytes` reject null or unmapped addresses before copying memory. They cannot prove the address is semantically safe; wrong instruction bytes or patching the wrong function can still crash the process.
- C++ wrappers are inline and call the C ABI. Do not depend on C++ STL symbols as plugin ABI.
- The previous `pl::patch::*` C++ symbols are still exported for already built mods. Treat them as legacy runtime compatibility only.
