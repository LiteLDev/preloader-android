# Getting Started

This page creates a minimal native mod and explains how preloader loads it.

## Purpose

Preloader reads `manifest.json`, loads the `.so` pointed to by `entry`, resolves the exported `LeviMod_Load` symbol, and calls it.

## Directory Layout

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

| Field | Purpose | Required |
| --- | --- | --- |
| `type` | Must be `preload-native` | Yes |
| `entry` | Safe relative path to the mod `.so` | Yes |
| `name` | Display name; directory name is used when empty | No |
| `author` | Author | No |
| `version` | Version | No |
| `icon` | Relative icon path; invalid values are ignored | No |
| `minecraft_versions` | Compatible Minecraft versions. Exact strings and `*` prefix wildcards are supported, such as `1.26.2*` matching `1.26.20`, `1.26.21`, and `1.26.22`. Missing or empty values mean all versions are allowed. | No |

## C Entry Example

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

## C++ Entry Example

```cpp
#include <android/log.h>
#include <pl/cpp/Mod.hpp>

extern "C" void LeviMod_Load(JavaVM *vm, const PLModInfo *mod_info) {
  (void)vm;
  __android_log_print(ANDROID_LOG_INFO, "ExampleMod", "Loaded %s",
                      mod_info ? mod_info->mod_id : "unknown");
}
```

## Common Mistakes

- Missing `extern "C"` for `LeviMod_Load` in C++.
- `manifest.json` uses a `type` other than `preload-native`.
- `entry` is absolute or contains `..`.
- `entry` does not point to a `.so` file.
