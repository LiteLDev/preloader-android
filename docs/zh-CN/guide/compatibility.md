# 兼容性

## 作用

本页说明 preloader 对外承诺稳定的 ABI、头文件路径和迁移建议。

## ABI 基线

当前 ABI 兼容以当前工作树为基线。文档和接口以当前仍存在的导出符号为准。

稳定导出包括：

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

为了兼容已经编译好的旧 mod，仍会保留旧的 `pl::patch::*` C++ 符号导出。新 mod 不应把这些符号当作稳定 ABI；请使用 `pl/c/Patch.h` 或 `pl/cpp/Patch.hpp` 的 inline wrapper。

旧版已经删除的 `ANativeActivity_onCreate`、`android_main`、`nativeOnLauncherLoaded` 代理逻辑不属于当前 ABI 承诺。

## 结构体兼容

`PLModInfo` 字段顺序和字段类型保持稳定。新增字段只能在结构体末尾追加，并应通过 `size` 做版本判断。

`PreloaderInput_Interface` 的函数指针顺序也应保持稳定。

## 头文件路径

推荐新路径：

| 场景 | 路径 |
| --- | --- |
| C ABI | `pl/c/*.h` |
| C++ 封装 | `pl/cpp/*.hpp` |
| Hook 宏 | `pl/api/memory/Hook.h` |
| Patch C ABI | `pl/c/Patch.h` |
| Patch C++ 封装 | `pl/cpp/Patch.hpp` |
| 旧宏兼容 | `pl/api/Macro.h` |
| 旧类型兼容 | `pl/api/Types.h` |

## 迁移建议

- C mod 从 `pl/Mod.h` 迁移到 `pl/c/Mod.h`。
- C++ mod 从 `pl/Hook.h` 迁移到 `pl/cpp/Hook.hpp`。
- C mod 使用 `pl/c/Patch.h` 调用 patch API。
- C++ mod 使用 `pl/cpp/Patch.hpp` 调用 patch wrapper。
- 不要在 C 代码中 include `pl/cpp/*.hpp`。
- 不要依赖内部目录 `pl/runtime`、`pl/internal`、`pl/memory` 的实现细节。
