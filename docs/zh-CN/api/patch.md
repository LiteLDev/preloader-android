# Patch API

## 作用

Patch API 用于读取、写入和回滚进程内存。C API 是稳定 ABI 边界；C++ API 是基于 C ABI 的 inline 便利封装。

## 头文件

C：

```c
#include <pl/c/Patch.h>
```

C++：

```cpp
#include <pl/cpp/Patch.hpp>
```

## 类型签名

C：

```c
bool pl_patch_write_bytes(uintptr_t addr, const uint8_t *bytes,
                          size_t len, const char *name);
bool pl_patch_write_hex(uintptr_t addr, const char *bytes,
                        const char *name);
size_t pl_patch_read_bytes(uintptr_t addr, uint8_t *out, size_t len);
bool pl_patch_revert(const char *name);
void pl_patch_revert_all(void);
```

C++：

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

### 作用

把字节写入指定地址，并用 `name` 保存原始字节，方便后续回滚。

### 参数

| 参数 | 说明 |
| --- | --- |
| `addr` | 要写入的地址 |
| `bytes` | 字节 buffer 或十六进制字节字符串，例如 `"00 00 80 D2"` |
| `len` | buffer 字节数 |
| `name` | patch 名称，用于回滚 |

### 返回值

返回 `true` 表示写入成功。字节为空、hex 字符串非法、`addr` 为 `0`、目标范围不可读、地址范围溢出或内存权限修改失败时返回 `false`。

### C 示例

```c
#include <pl/c/Patch.h>

const uint8_t bytes[] = {0x00, 0x00, 0x80, 0xD2, 0xC0, 0x03, 0x5F, 0xD6};
bool ok = pl_patch_write_bytes(address, bytes, sizeof(bytes), "return_zero");
```

### C++ 示例

```cpp
#include <pl/cpp/Patch.hpp>

bool ok = pl::patch::writeBytes(address, "00 00 80 D2 C0 03 5F D6",
                                "return_zero");
```

## pl_patch_read_bytes / readBytes

### 作用

读取指定地址的字节。

### 参数

| 参数 | 说明 |
| --- | --- |
| `addr` | 起始地址 |
| `out` | 调用方提供的输出 buffer |
| `len` | 读取长度 |

### 返回值

C API 返回实际读取字节数。`out` 为 `NULL`、`addr` 为 `0`、`len` 为 `0`、地址范围溢出或目标范围不可读时返回 `0`。

C++ wrapper 返回读取到的字节数组，失败时返回空数组。

## pl_patch_revert / revert

### 作用

按名称回滚单个 patch。

### 参数

| 参数 | 说明 |
| --- | --- |
| `name` | 写入 patch 时使用的名称 |

### 返回值

返回 `true` 表示回滚成功，返回 `false` 表示名称不存在或内存权限修改失败。

## pl_patch_revert_all / revertAll

### 作用

回滚当前记录的全部 patch。

### 参数与返回值

无参数，无返回值。

## 注意事项

- 同名 patch 会覆盖旧记录；需要多个独立 patch 时使用不同名称。
- 写入前会保存原始字节，保存长度等于本次写入长度。
- hex 字符串使用空白分隔字节，每个字节 token 必须是一到两个十六进制字符。
- `writeBytes` 和 `readBytes` 会在复制内存前拒绝空地址或未映射地址，但这只能避免明显的空指针崩溃，不能证明 patch 在语义上安全。
- patch 地址和字节必须匹配目标 ABI 指令集；写错函数或写错指令仍会导致进程崩溃。
- C++ wrapper 是 inline 封装并调用 C ABI，不要把 C++ STL 符号当作插件 ABI 依赖。
- 旧的 `pl::patch::*` C++ 符号仍会导出，用于兼容已经编译好的旧 mod；它们只属于 legacy 运行时兼容。
