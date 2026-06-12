# Compatibility

## Purpose

This page documents ABI, header path, and migration compatibility.

## ABI Baseline

Compatibility is based on the current working tree API. Stable exports include:

- `JNI_OnLoad`
- `Java_org_levimc_launcher_core_mods_ModManager_nativeLoadMod`
- `Java_org_levimc_launcher_preloader_PreloaderInput_nativeOnTouch`
- `Java_org_levimc_launcher_preloader_PreloaderInput_nativeOnKeyEvent`
- `Java_org_levimc_launcher_preloader_PreloaderInput_nativeSetActivity`
- `Java_org_levimc_launcher_preloader_PreloaderInput_nativeClearActivity`
- `GetPreloaderInput`
- `pl_hook`
- `pl_unhook`
- `pl_patch_write_bytes`
- `pl_patch_write_hex`
- `pl_patch_read_bytes`
- `pl_patch_revert`
- `pl_patch_revert_all`
- `pl_resolve_signature`

Legacy binary compatibility exports also include the previous `pl::patch::*`
C++ symbols used by already built mods. New mods should not use these as a
stable ABI; use `pl/c/Patch.h` or the inline wrappers in `pl/cpp/Patch.hpp`.

Removed legacy `ANativeActivity_onCreate`, `android_main`, and `nativeOnLauncherLoaded` proxy behavior is not part of the current ABI contract.

## Structure Compatibility

`PLModInfo` field order and field types are stable. New fields should only be appended and guarded by `size`.

`PreloaderInput_Interface` function pointer order should also remain stable.

## Header Paths

Recommended paths:

| Use case | Path |
| --- | --- |
| C ABI | `pl/c/*.h` |
| C++ wrappers | `pl/cpp/*.hpp` |
| Hook macros | `pl/api/memory/Hook.h` |
| Patch C ABI | `pl/c/Patch.h` |
| Patch C++ wrapper | `pl/cpp/Patch.hpp` |
| Legacy macro compatibility | `pl/api/Macro.h` |
| Legacy type compatibility | `pl/api/Types.h` |

## Migration

- C mods should migrate from `pl/Mod.h` to `pl/c/Mod.h`.
- C++ mods should migrate from `pl/Hook.h` to `pl/cpp/Hook.hpp`.
- C mods should use `pl/c/Patch.h` for patch APIs.
- C++ mods should use `pl/cpp/Patch.hpp` for patch wrappers.
- Do not include `pl/cpp/*.hpp` from C code.
- Do not depend on internal directories such as `pl/runtime`, `pl/internal`, or `pl/memory`.
