# Mod Entry API

## Purpose

The Mod Entry API defines how preloader passes mod metadata and `JavaVM` to a native mod. Every native mod must export `LeviMod_Load`.

## Headers

C:

```c
#include <pl/c/Mod.h>
```

C++:

```cpp
#include <pl/cpp/Mod.hpp>
```

## Signatures

```c
typedef struct PLModInfo {
  uint32_t size;
  const char *mod_id;
  const char *display_name;
  const char *author;
  const char *version;
  const char *entry_path;
  const char *entry_file_name;
  const char *library_path;
  const char *icon_path;
  const char *manifest_path;
  const char *mod_root_path;
} PLModInfo;

typedef void (*PLModLoadFunc)(JavaVM *vm, const PLModInfo *mod_info);
```

The mod must export:

```c
void LeviMod_Load(JavaVM *vm, const PLModInfo *mod_info);
```

## Fields

| Field | Purpose |
| --- | --- |
| `size` | Size of the current structure for compatibility checks |
| `mod_id` | Mod directory name |
| `display_name` | Manifest `name`; directory name is used when empty |
| `author` | Manifest `author` |
| `version` | Manifest `version` |
| `entry_path` | Manifest entry relative path |
| `entry_file_name` | Entry `.so` file name |
| `library_path` | Actual loaded `.so` path |
| `icon_path` | Valid icon relative path, or empty |
| `manifest_path` | Manifest file path |
| `mod_root_path` | Mod root directory |

## Parameters

| Parameter | Description |
| --- | --- |
| `vm` | Current process `JavaVM *` |
| `mod_info` | Current mod metadata |

## Return Value

`LeviMod_Load` returns nothing.

## Example

```c
#include <pl/c/Mod.h>

void LeviMod_Load(JavaVM *vm, const PLModInfo *mod_info) {
  (void)vm;
  if (!mod_info || mod_info->size < sizeof(PLModInfo)) {
    return;
  }

  const char *id = mod_info->mod_id;
  (void)id;
}
```

## Notes

- Strings inside `PLModInfo` are owned by preloader and should be copied if stored.
- C++ mods must export `LeviMod_Load` with `extern "C"`.
- Do not mutate data pointed to by `PLModInfo`.

