# Build

## Purpose

This page explains include paths and Android `.so` build setup for mods.

## Supported ABIs

- `arm64-v8a`
- `armeabi-v7a`

Other ABIs are not part of the compatibility contract.

## Include Paths

```cmake
target_include_directories(your_mod PRIVATE path/to/preloader/src)
```

C:

```c
#include <pl/c/Mod.h>
#include <pl/c/Hook.h>
#include <pl/c/Signature.h>
```

C++:

```cpp
#include <pl/cpp/Mod.hpp>
#include <pl/cpp/Hook.hpp>
#include <pl/cpp/Patch.hpp>
#include <pl/cpp/Signature.hpp>
```

Legacy:

```cpp
#include <pl/Mod.h>
#include <pl/Hook.h>
#include <pl/Patch.h>
#include <pl/Signature.h>
```

## Minimal CMake Example

```cmake
cmake_minimum_required(VERSION 3.22)
project(example_mod LANGUAGES C CXX)

add_library(example SHARED src/example.cpp)

target_include_directories(example PRIVATE
    path/to/preloader/src)
```

## Android NDK Build Example

```powershell
cmake -S . -B build-arm64 `
  -DCMAKE_TOOLCHAIN_FILE="$env:ANDROID_HOME\ndk\<version>\build\cmake\android.toolchain.cmake" `
  -DANDROID_ABI=arm64-v8a `
  -DANDROID_PLATFORM=android-23 `
  -G Ninja

cmake --build build-arm64
```

## Common Errors

- `jni.h` not found: build with Android NDK toolchain.
- `pl/c/Mod.h` not found: include preloader `src` or installed include directory.
- Built only x86 ABI: preloader supports ARM ABIs only.
- `LeviMod_Load` missing in C++: export it with `extern "C"`.
