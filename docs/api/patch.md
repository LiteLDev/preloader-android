# Patch API

## Purpose

Patch API reads, writes, and reverts process memory. It is a C++ API designed for known addresses.

## Header

```cpp
#include <pl/Patch.h>
```

## Signatures

```cpp
struct patchInfo {
  uintptr_t address;
  std::vector<uint8_t> bytes;
};

namespace pl::patch {
bool writeBytes(uintptr_t addr, const std::string &bytes_str,
                const std::string &name);
bool writeBytes(uintptr_t addr, const std::vector<uint8_t> &bytes,
                const std::string &name);
std::vector<uint8_t> readBytes(uintptr_t addr, size_t len);
bool revert(const std::string &name);
void revertAll();
}
```

## writeBytes

### Purpose

Writes bytes to an address and stores original bytes under `name`.

### Parameters

| Parameter | Description |
| --- | --- |
| `addr` | Target address |
| `bytes_str` | Hex byte string, such as `"00 00 80 D2"` |
| `bytes` | Byte vector |
| `name` | Patch name for later revert |

### Return Value

Returns `true` on success. Returns `false` when bytes are empty or memory permission changes fail.

### Example

```cpp
#include <pl/Patch.h>

bool ok = pl::patch::writeBytes(address, "00 00 80 D2 C0 03 5F D6",
                                "return_zero");
```

## readBytes

### Purpose

Reads bytes from an address.

### Parameters

| Parameter | Description |
| --- | --- |
| `addr` | Start address |
| `len` | Number of bytes |

### Return Value

Returns the read bytes.

## revert

### Purpose

Reverts one named patch.

### Return Value

Returns `true` on success, otherwise `false`.

## revertAll

### Purpose

Reverts all recorded patches.

## Notes

- Reusing a patch name overwrites the previous record.
- Saved original byte length equals the write length.
- Wrong addresses or instruction bytes can crash the process.

