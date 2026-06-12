# Patch API

## 作用

Patch API 用于读取、写入和回滚进程内存。它是 C++ API，适合对已知地址写入指令或数据。

## 头文件

推荐路径：

```cpp
#include <pl/cpp/Patch.hpp>
```

旧兼容包装头：

```cpp
#include <pl/Patch.h>
```

## 类型签名

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

### 作用

把字节写入指定地址，并用 `name` 保存原始字节，方便后续回滚。

### 参数

| 参数 | 说明 |
| --- | --- |
| `addr` | 要写入的地址 |
| `bytes_str` | 十六进制字节字符串，例如 `"00 00 80 D2"` |
| `bytes` | 要写入的字节数组 |
| `name` | patch 名称，用于回滚 |

### 返回值

返回 `true` 表示写入成功。字节为空、`addr` 为 `0`、目标范围不可读、地址范围溢出或内存权限修改失败时返回 `false`。

### 示例

```cpp
#include <pl/cpp/Patch.hpp>

bool ok = pl::patch::writeBytes(address, "00 00 80 D2 C0 03 5F D6",
                                "return_zero");
```

## readBytes

### 作用

读取指定地址的字节。

### 参数

| 参数 | 说明 |
| --- | --- |
| `addr` | 起始地址 |
| `len` | 读取长度 |

### 返回值

返回读取到的字节数组。`addr` 为 `0`、`len` 为 `0`、地址范围溢出或目标范围不可读时返回空数组。

## revert

### 作用

按名称回滚单个 patch。

### 参数

| 参数 | 说明 |
| --- | --- |
| `name` | `writeBytes` 时使用的 patch 名称 |

### 返回值

返回 `true` 表示回滚成功，返回 `false` 表示名称不存在或内存权限修改失败。

## revertAll

### 作用

回滚当前记录的全部 patch。

### 参数与返回值

无参数，无返回值。

## 注意事项

- 同名 patch 会覆盖旧记录；需要多个独立 patch 时使用不同名称。
- 写入前会保存原始字节，保存长度等于本次写入长度。
- `writeBytes` 和 `readBytes` 会在复制内存前拒绝空地址或未映射地址，但这只能避免明显的空指针崩溃，不能证明 patch 在语义上安全。
- patch 地址和字节必须匹配目标 ABI 指令集；写错函数或写错指令仍会导致进程崩溃。
- `pl/Patch.h` 仍保留用于源码兼容，新 C++ mod 推荐 include `pl/cpp/Patch.hpp`。
