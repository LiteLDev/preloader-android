# 构建

## 作用

本页说明如何在 mod 中 include preloader 头文件，以及如何构建 Android `.so`。

## 支持 ABI

preloader 当前支持：

- `arm64-v8a`
- `armeabi-v7a`

其它 ABI 不在兼容范围内。

## include 路径

如果你的构建系统能访问 preloader 的 `src` 目录：

```cmake
target_include_directories(your_mod PRIVATE path/to/preloader/src)
```

C mod 推荐：

```c
#include <pl/c/Mod.h>
#include <pl/c/Hook.h>
#include <pl/c/Signature.h>
```

C++ mod 推荐：

```cpp
#include <pl/cpp/Mod.hpp>
#include <pl/cpp/Hook.hpp>
#include <pl/cpp/Signature.hpp>
```

旧代码仍可使用：

```cpp
#include <pl/Mod.h>
#include <pl/Hook.h>
#include <pl/Signature.h>
```

## 最小 CMake 示例

```cmake
cmake_minimum_required(VERSION 3.22)
project(example_mod LANGUAGES C CXX)

add_library(example SHARED src/example.cpp)

target_include_directories(example PRIVATE
    path/to/preloader/src)
```

## Android NDK 构建示例

```powershell
cmake -S . -B build-arm64 `
  -DCMAKE_TOOLCHAIN_FILE="$env:ANDROID_HOME\ndk\<version>\build\cmake\android.toolchain.cmake" `
  -DANDROID_ABI=arm64-v8a `
  -DANDROID_PLATFORM=android-23 `
  -G Ninja

cmake --build build-arm64
```

## 常见错误

- 找不到 `jni.h`：确认使用 Android NDK toolchain 构建。
- 找不到 `pl/c/Mod.h`：确认 include 路径指向 preloader 的 `src` 目录或安装后的 include 目录。
- 只构建了 x86 ABI：preloader 不支持 x86/x86_64。
- C++ mod 的 `LeviMod_Load` 没有 `extern "C"`：会导致 preloader 找不到入口符号。
