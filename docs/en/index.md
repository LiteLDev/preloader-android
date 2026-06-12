# Preloader Android

Preloader Android is the native mod preloading API used by LeviLaunchroid. It exposes a stable C ABI and C++ convenience wrappers for mod entry points, input callbacks, function hooks, signature lookup, and memory patches.

This documentation focuses on public API usage. Each API page explains purpose, headers, signatures, parameters, return values, examples, and common pitfalls.

[简体中文文档](../index.md)

## Minimal Native Mod

```c
#include <pl/c/Mod.h>

void LeviMod_Load(JavaVM *vm, const PLModInfo *mod_info) {
  (void)vm;
  (void)mod_info;
}
```

Matching `manifest.json`:

```json
{
  "type": "preload-native",
  "name": "Example Mod",
  "entry": "libexample.so"
}
```

## Recommended Reading Order

1. Start with [Getting Started](getting-started.md).
2. Read [Mod Entry](api/mod.md) to understand `PLModInfo` and `LeviMod_Load`.
3. Use [Input API](api/input.md) for touch, keyboard, and soft keyboard integration.
4. Use [Hook API](api/hook.md) and [Signature API](api/signature.md) for function replacement.
5. Check [Build](build.md) and [Compatibility](compatibility.md) before shipping.

## C and C++ Headers

- C mods should include `pl/c/*.h`.
- C++ mods should include `pl/cpp/*.hpp`.
- Legacy `pl/*.h` headers remain available as compatibility wrappers.

