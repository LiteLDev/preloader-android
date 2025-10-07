cmake_minimum_required(VERSION 3.22.1)
project(preloader)


file(GLOB_RECURSE PRELOADER_SRC
        "src/**/*.cpp"
)



add_library(preloader SHARED ${PRELOADER_SRC})


target_include_directories(preloader
        PRIVATE src
)

target_compile_definitions(preloader
        PRIVATE PRELOADER_EXPORT UNICODE
)


include(FetchContent)

FetchContent_Declare(
        nlohmann_json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(nlohmann_json)

FetchContent_Declare(
        fmt
        GIT_REPOSITORY https://github.com/fmtlib/fmt.git
        GIT_TAG 10.2.1
)
FetchContent_MakeAvailable(fmt)

find_library(log-lib log)
find_library(android-lib android)
find_library(egl-lib EGL)
find_library(gles-lib GLESv3)


if(CMAKE_ANDROID_ARCH_ABI STREQUAL "armeabi-v7a")
    set(GLOSSHOOK_PATH "${CMAKE_CURRENT_SOURCE_DIR}/lib/ARM/libGlossHook.a")
elseif(CMAKE_ANDROID_ARCH_ABI STREQUAL "arm64-v8a")
    set(GLOSSHOOK_PATH "${CMAKE_CURRENT_SOURCE_DIR}/lib/ARM64/libGlossHook.a")
else()
    message(FATAL_ERROR "Unsupported ABI: ${CMAKE_ANDROID_ARCH_ABI}")
endif()


target_link_libraries(preloader
        PRIVATE
        ${GLOSSHOOK_PATH}
        ${log-lib}
        ${android-lib}
        ${egl-lib}
        ${gles-lib}
        nlohmann_json::nlohmann_json
        fmt::fmt
)
