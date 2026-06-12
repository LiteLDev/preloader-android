---
layout: home

hero:
  name: Preloader Android
  text: Native mod preloading API for LeviLaunchroid
  tagline: Build preload-native mods with a stable C ABI, C++ wrappers, input callbacks, function hooks, signature lookup, and memory patches.
  image:
    src: /appicon.png
    alt: Preloader Android
  actions:
    - theme: brand
      text: Get started
      link: /guide/getting-started
    - theme: alt
      text: API reference
      link: /api/mod

features:
  - title: Stable C ABI
    details: Use small C headers for mod entry points, input callbacks, hooks, and signature lookup.
  - title: C++ convenience layer
    details: Prefer typed wrappers and helper macros when writing C++ mods.
  - title: Runtime input bridge
    details: Register touch and key callbacks, then request the Android soft keyboard when needed.
  - title: Hook workflow
    details: Resolve target addresses, install prioritized detours, and keep original calls available.
  - title: Signature lookup
    details: Resolve symbols or scan byte patterns inside loaded modules.
  - title: Patch utilities
    details: Write, read, and revert named memory patches for known addresses.
---

::: tip Looking for Chinese documentation?
Use the language selector or open [简体中文](/zh-CN/).
:::

## What this site covers

This site is written for native mod developers who use Preloader Android.

- Declare `manifest.json` and export `LeviMod_Load`.
- Read `PLModInfo` metadata safely.
- Register touch and key callbacks.
- Hook functions and call originals.
- Resolve symbols or byte signatures.
- Write and revert memory patches.

## Minimal native mod

```c
#include <pl/c/Mod.h>

void LeviMod_Load(JavaVM *vm, const PLModInfo *mod_info) {
  (void)vm;
  (void)mod_info;
}
```

## Recommended reading order

1. [Getting Started](/guide/getting-started)
2. [Mod Entry API](/api/mod)
3. [Input API](/api/input)
4. [Hook API](/api/hook)
5. [Build](/guide/build)
