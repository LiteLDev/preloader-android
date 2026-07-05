cmake_minimum_required(VERSION 3.22)
project(preloader LANGUAGES CXX)

if(NOT ANDROID)
    message(FATAL_ERROR
            "preloader is an Android runtime target. "
            "Use it only from an Android toolchain build.")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(PRELOADER_SOURCES
        src/pl/Mod.cpp
        src/pl/PreLoader.cpp
        src/pl/internal/LoadedModRegistry.cpp
        src/pl/internal/ModManifest.cpp
        src/pl/internal/ModManager.cpp
        src/pl/internal/NativeModLifecycle.cpp
        src/pl/legacy/LegacyHook.cpp
        src/pl/legacy/LegacyInput.cpp
        src/pl/legacy/LegacyPatch.cpp
        src/pl/legacy/LegacySignature.cpp
        src/pl/memory/Hook.cpp
        src/pl/memory/Patch.cpp
        src/pl/memory/Signature.cpp
        src/pl/memory/Vtable.cpp
        src/pl/runtime/GameHookRules.cpp
        src/pl/runtime/GameHooks.cpp
        src/pl/runtime/ExternalModBridgeJni.cpp
        src/pl/runtime/InputBridge.cpp
        src/pl/runtime/InputJni.cpp
        src/pl/runtime/JavaRuntime.cpp
        src/pl/runtime/ModMenuBridge.cpp
)

add_library(preloader SHARED ${PRELOADER_SOURCES})

target_include_directories(preloader
        PUBLIC
        include
        PRIVATE
        src)
target_compile_definitions(preloader PRIVATE PRELOADER_EXPORT UNICODE)
target_compile_options(preloader PRIVATE
        $<$<COMPILE_LANG_AND_ID:CXX,Clang,GNU>:-fvisibility=hidden -fvisibility-inlines-hidden>)

include(FetchContent)
FetchContent_Declare(
        nlohmann_json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG v3.11.3
)
FetchContent_Declare(
        boost_pfr
        GIT_REPOSITORY https://github.com/boostorg/pfr.git
        GIT_TAG boost-1.88.0
)
FetchContent_Declare(
        magic_enum
        GIT_REPOSITORY https://github.com/Neargye/magic_enum.git
        GIT_TAG v0.9.7
)
FetchContent_Declare(
        fmt
        GIT_REPOSITORY https://github.com/fmtlib/fmt.git
        GIT_TAG 11.2.0
)
FetchContent_Declare(
        lunasvg
        GIT_REPOSITORY https://github.com/sammycage/lunasvg.git
        GIT_TAG v3.5.0
        EXCLUDE_FROM_ALL
)
set(FMT_DOC OFF CACHE BOOL "Disable fmt docs" FORCE)
set(FMT_TEST OFF CACHE BOOL "Disable fmt tests" FORCE)
set(FMT_INSTALL OFF CACHE BOOL "Disable fmt install rules" FORCE)
set(LUNASVG_BUILD_EXAMPLES OFF CACHE BOOL "Disable lunasvg examples" FORCE)
set(LUNASVG_DISABLE_LOAD_SYSTEM_FONTS ON CACHE BOOL "Disable lunasvg system font scan" FORCE)

set(_preloader_build_shared_libs ${BUILD_SHARED_LIBS})
set(BUILD_SHARED_LIBS OFF)
FetchContent_MakeAvailable(nlohmann_json boost_pfr magic_enum fmt lunasvg)
set(BUILD_SHARED_LIBS ${_preloader_build_shared_libs})

find_library(log-lib log REQUIRED)
find_library(android-lib android REQUIRED)
find_library(egl-lib EGL REQUIRED)
find_library(gles-lib GLESv3 REQUIRED)

if(CMAKE_ANDROID_ARCH_ABI STREQUAL "armeabi-v7a")
    set(GLOSSHOOK_PATH "${CMAKE_CURRENT_SOURCE_DIR}/lib/ARM/libGlossHook.a")
elseif(CMAKE_ANDROID_ARCH_ABI STREQUAL "arm64-v8a")
    set(GLOSSHOOK_PATH "${CMAKE_CURRENT_SOURCE_DIR}/lib/ARM64/libGlossHook.a")
else()
    message(FATAL_ERROR
            "Unsupported Android ABI for preloader: ${CMAKE_ANDROID_ARCH_ABI}")
endif()

if(NOT EXISTS "${GLOSSHOOK_PATH}")
    message(FATAL_ERROR
            "GlossHook static library not found for ${CMAKE_ANDROID_ARCH_ABI}: "
            "${GLOSSHOOK_PATH}")
endif()

target_link_libraries(preloader
        PUBLIC
        fmt::fmt
        PRIVATE
        ${GLOSSHOOK_PATH}
        ${log-lib}
        ${android-lib}
        ${egl-lib}
        ${gles-lib}
        nlohmann_json::nlohmann_json
        Boost::pfr
        magic_enum::magic_enum
        lunasvg::lunasvg
)

install(TARGETS preloader
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION lib
        RUNTIME DESTINATION bin)

install(DIRECTORY include/pl
        DESTINATION include
        FILES_MATCHING PATTERN "*.hpp")
