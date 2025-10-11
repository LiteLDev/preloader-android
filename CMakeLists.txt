cmake_minimum_required(VERSION 3.22)
project(preloader LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

file(GLOB_RECURSE PRELOADER_SRC "src/**/*.cpp")
add_library(preloader SHARED ${PRELOADER_SRC})

target_include_directories(preloader PRIVATE src)
target_compile_definitions(preloader PRIVATE PRELOADER_EXPORT UNICODE)

include(FetchContent)
FetchContent_Declare(
        nlohmann_json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(nlohmann_json)

FetchContent_Declare(
        Dobby
        GIT_REPOSITORY https://github.com/dzyong/Dobby.git
        GIT_TAG master
)
FetchContent_MakeAvailable(Dobby)

find_library(log-lib log)
find_library(android-lib android)
find_library(egl-lib EGL)
find_library(gles-lib GLESv3)

target_link_libraries(preloader
        PRIVATE
        ${log-lib}
        ${android-lib}
        ${egl-lib}
        ${gles-lib}
        nlohmann_json::nlohmann_json
        dobby
)