# 快速开始

本页从零开始创建一个最小 native mod，并说明它如何被 preloader 加载。

## 作用

preloader 会读取 mod 目录中的 `manifest.json`，加载 `entry` 指向的 `.so`，查找导出符号 `LeviMod_Load` 并调用它。

## 目录结构

```text
example-mod/
├── manifest.json
└── libexample.so
```

## manifest.json

```json
{
  "type": "preload-native",
  "name": "Example Mod",
  "author": "LiteLDev",
  "version": "1.0.0",
  "entry": "libexample.so",
  "minecraft_versions": ["1.26.20", "1.26.2*", "1.26.*"]
}
```

字段说明：

| 字段 | 作用 | 必填 |
| --- | --- | --- |
| `type` | 必须是 `preload-native` | 是 |
| `entry` | mod 入口 `.so` 的安全相对路径 | 是 |
| `name` | 显示名称；为空时使用目录名 | 否 |
| `author` | 作者 | 否 |
| `version` | 版本 | 否 |
| `icon` | 图标相对路径；无效时会被忽略 | 否 |
| `minecraft_versions` | 兼容的 Minecraft 版本。支持精确字符串和 `*` 前缀通配，例如 `1.26.2*` 会匹配 `1.26.20`、`1.26.21`、`1.26.22`。缺失或为空表示兼容所有版本。 | 否 |

## C 入口示例

```c
#include <android/log.h>
#include <pl/c/Mod.h>

void LeviMod_Load(JavaVM *vm, const PLModInfo *mod_info) {
  (void)vm;

  const char *name = mod_info && mod_info->display_name
                         ? mod_info->display_name
                         : "unknown";
  __android_log_print(ANDROID_LOG_INFO, "ExampleMod", "Loaded %s", name);
}
```

## C++ 入口示例

```cpp
#include <android/log.h>
#include <pl/cpp/Mod.hpp>

extern "C" void LeviMod_Load(JavaVM *vm, const PLModInfo *mod_info) {
  (void)vm;
  __android_log_print(ANDROID_LOG_INFO, "ExampleMod", "Loaded %s",
                      mod_info ? mod_info->mod_id : "unknown");
}
```

## 常见错误

- `LeviMod_Load` 没有用 C ABI 导出：C++ 示例必须写 `extern "C"`。
- `manifest.json` 的 `type` 不是 `preload-native`：preloader 会跳过这个目录。
- `entry` 是绝对路径或包含 `..`：会被当成不安全路径拒绝。
- `entry` 不是 `.so` 文件：加载会失败。
