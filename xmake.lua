add_rules("mode.debug", "mode.release")

add_requires("nlohmann_json v3.11.3")
add_requires("fmt 10")

target("preloader")
    set_kind("shared")
    set_symbols("none")
    set_strip("all")
    set_languages("c++20")
    set_exceptions("none")
    add_headerfiles("src/(**.h)")
    add_files("src/**.cpp")
    add_includedirs("./src")

    if is_arch("armeabi-v7a") then
        add_linkdirs("lib/ARM")
    elseif is_arch("arm64-v8a") then
        add_linkdirs("lib/ARM64")
    end
    
    add_links("GlossHook")
    add_defines("PRELOADER_EXPORT", "UNICODE")
    add_packages("nlohmann_json", "fmt")