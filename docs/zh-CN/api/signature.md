# Signature API

## 作用

Signature API 用于在已加载模块中解析符号名或字节特征码，返回匹配地址。

## 头文件

C:

```c
#include <pl/c/Signature.h>
```

C++:

```cpp
#include <pl/cpp/Signature.hpp>
```

## 类型签名

```c
PLAPI uintptr_t pl_resolve_signature(const char *signature,
                                     const char *moduleName);
```

C++：

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

### 作用

优先按符号名解析；如果符号不存在，则把 `signature` 当作字节 pattern 扫描模块可读内存区域。

### 参数

| 参数 | 说明 |
| --- | --- |
| `signature` | 符号名或字节 pattern，不能为 `NULL` |
| `moduleName` | 模块名或路径片段，不能为 `NULL` |

### 返回值

返回匹配地址。失败返回 `0`。

## Pattern 格式

支持空格分隔或连续十六进制字节：

```text
48 8B ?? ?? 89
488B????89
```

通配符：

| 写法 | 说明 |
| --- | --- |
| `?` | 整字节通配 |
| `??` | 整字节通配 |
| `A?` | 低 4 bit 通配 |
| `?F` | 高 4 bit 通配 |

## C 示例

```c
#include <pl/c/Signature.h>

uintptr_t addr = pl_resolve_signature("SomeSymbol", "libminecraftpe.so");
if (addr == 0) {
  addr = pl_resolve_signature("48 8B ?? ?? 89", "libminecraftpe.so");
}
```

## C++ 批量示例

```cpp
#include <pl/cpp/Signature.hpp>

auto results = pl::signature::resolveSignatures(
    {"SymbolA", "48 8B ?? ?? 89"},
    "libminecraftpe.so");

uintptr_t symbolA = results["SymbolA"];
```

## 注意事项

- `moduleName` 必须能匹配 `/proc/self/maps` 中的模块路径。
- pattern 为空或格式非法时返回 `0`。
- 结果会缓存；模块重新加载或内存布局变化时不要假设旧地址仍有效。
- 批量解析多个 signature 时优先使用 `resolveSignatures`，避免重复扫描。
