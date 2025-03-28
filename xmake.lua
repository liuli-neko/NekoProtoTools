set_project("neko-proto")
add_rules("mode.debug", "mode.release", "mode.coverage")
set_version("0.1.0", {build = "%Y%m%d%H%M"})
add_repositories("btk-repo https://github.com/Btk-Project/xmake-repo.git")
set_warnings("allextra")

add_configfiles("include/nekoproto/proto/private/config.h.in")
set_configdir("include/nekoproto/proto/private")

option("enable_simdjson")
    set_default(false)
    set_showmenu(true)
    set_description("Enable simdjson support, should install simdjson")
    set_category("serializer provider")
    set_configvar("NEKO_PROTO_ENABLE_SIMDJSON", true)
option_end()

option("enable_rapidjson")
    set_default(false)
    set_showmenu(true)
    set_description("Enable rapidjson support, should install rapidjson")
    set_category("serializer provider")
    set_configvar("NEKO_PROTO_ENABLE_RAPIDJSON", true)
option_end()

option("enable_rapidxml")
    set_default(false)
    set_showmenu(true)
    set_description("Enable rapidxml support, should install rapidxml")
    set_category("serializer provider")
    set_configvar("NEKO_PROTO_ENABLE_RAPIDXML", true)
option_end()

option("enable_spdlog")
    set_default(false)
    set_showmenu(true)
    set_description("Enable spdlog for log, should install spdlog")
    set_category("log provider")
    set_configvar("NEKO_PROTO_USE_SPDLOG", true)
    after_check(function (option)
        if has_config("enable_spdlog") and has_config("enable_fmt") then 
            assert(false, "spdlog can not be used with fmt or stdformat")
        end
    end)
option_end()

option("enable_fmt")
    set_default(false)
    set_showmenu(true)
    set_description("Enable fmt for log, should install fmt")
    set_category("log provider")
    set_configvar("NEKO_PROTO_USE_FMT", true)
    after_check(function (option)
        if has_config("enable_fmt") and has_config("enable_spdlog") then 
            assert(false, "spdlog can not be used with fmt or stdformat")
        end
    end)
option_end()

option("enable_tests")
    set_default(false)
    set_showmenu(true)
    set_description("Enable test")
    set_category("enable test")
option_end()

option("enable_stdformat")
    set_showmenu(true)
    set_description("Enable std format for log, should support #include <format>")
    set_category("log provider")
    set_configvar("NEKO_PROTO_USE_STD_FORMAT", true)
    add_cxxincludes("format")
    set_values(true, false)
    after_check(function (option)
        if has_config("enable_spdlog") or has_config("enable_fmt") then 
            option:enable(false)
        end
    end)
option_end()

option("enable_communication")
    set_default(false)
    set_showmenu(true)
    set_description("Enable communication support, need ilias and proto module")
    set_category("modules")
option_end()

if has_config("enable_simdjson") then
    add_requires("simdjson v3.9.3", {configs = { shared = is_kind("shared")}})
    add_packages("simdjson")
end

if has_config("enable_rapidjson") then
    add_requires("rapidjson", {configs = { shared = is_kind("shared")}})
    add_packages("rapidjson")
end

if has_config("enable_rapidxml") then
    add_requires("rapidxml", {configs = { shared = is_kind("shared")}})
    add_packages("rapidxml")
end

if has_config("enable_spdlog") then
    add_requires("spdlog", {configs = { shared = is_kind("shared")}})
    add_packages("spdlog")
end

if has_config("enable_fmt") then
    add_requires("fmt", {configs = { shared = is_kind("shared")}})
    add_packages("fmt")
end

if has_config("enable_communication") then
    add_requires("ilias", {configs = { shared = is_kind("shared")}})
end

if has_config("enable_tests") then
    includes("tests")
end

if is_mode("debug") then
    add_defines("NEKO_PROTO_LOG_CONTEXT")
end 
add_defines("NEKO_PROTO_NLOG")

if is_plat("linux") then
    add_cxxflags("-fcoroutines")
end

target("NekoProtoBase")
    if is_kind("shared") then
        set_kind("shared")
        add_defines("NEKO_PROTO_LIBRARY")
    else
        set_kind("static")
        set_configvar("NEKO_PROTO_STATIC", true)
    end
    add_options("enable_spdlog", 
                "enable_fmt", 
                "enable_stdformat", 
                "enable_rapidjson", 
                "enable_simdjson", 
                "enable_rapidxml", 
                "enable_communication")
    add_headerfiles("include/(nekoproto/proto/**.hpp)")
    add_headerfiles("include/(nekoproto/proto/**.h)")
    add_includedirs("include")
    add_defines("ILIAS_COROUTINE_LIFETIME_CHECK")
    add_files("src/proto_base.cpp")
    if has_config("enable_communication") then
        add_headerfiles("include/(nekoproto/communication/**.hpp)")
        add_defines("ILIAS_COROUTINE_LIFETIME_CHECK")
        add_packages("ilias")
        add_files("src/communication_base.cpp")
        set_languages("c++20")
    else
        set_languages("c++17")
    end
target_end()

if is_mode("coverage") and is_plat("linux") then 
    target("coverage-report")
        set_kind("phony")
        -- 执行脚本命令
        on_run(function (target)
            print("Generating coverage report...")
            -- 在这里编写您想要执行的脚本命令
            os.execv("lcov ", {"--capture", 
                "--directory", os.projectdir() .. "/build/.objs/", 
                "--output-file", os.projectdir() .. "/build/coverage.info",
                "--exclude", "/usr/include/*",
                "--exclude", "/usr/local/include/*",
                "--exclude", "*.xmake/packages/*",
                "--exclude", "*modules/Ilias/*",
                "--exclude", "*/tests/test_*",
                "--directory", os.projectdir()})
            os.execv("genhtml ", {
                "--output-directory", os.projectdir() .. "/build/coverage-report",
                "--title", "'Coverage Report'", 
                os.projectdir() .. "/build/coverage.info"})
        end)
    target_end()
end 
