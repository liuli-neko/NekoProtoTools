set_project("neko-ccproto")
add_rules("mode.debug", "mode.release", "mode.valgrind", "mode.coverage")
set_version("1.0.0")

add_repositories("btk-repo git@github.com:Btk-Project/xmake-repo.git")

option("enable_simdjson")
    set_default(false)
    set_showmenu(true)
    set_description("Enable simdjson test, should install simdjson")
    set_category("serializer provider")
    add_defines("NEKO_PROTO_ENABLE_SIMDJSON")
option_end()

option("enable_rapidjson")
    set_default(false)
    set_showmenu(true)
    set_description("Enable rapidjson test, should install rapidjson")
    set_category("serializer provider")
    add_defines("NEKO_PROTO_ENABLE_RAPIDJSON")
option_end()

option("enable_spdlog")
    set_default(false)
    set_showmenu(true)
    set_description("Enable spdlog for log, should install spdlog")
    set_category("log provider")
    add_defines("NEKO_PROTO_USE_SPDLOG")
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
    add_defines("NEKO_PROTO_USE_FMT")
    after_check(function (option)
        if has_config("enable_fmt") and has_config("enable_spdlog") then 
            assert(false, "spdlog can not be used with fmt or stdformat")
        end
    end)
option_end()

option("enable_stdformat")
    set_showmenu(true)
    set_description("Enable std format for log, should support #include <format>")
    set_category("log provider")
    add_defines("NEKO_PROTO_USE_STD_FORMAT")
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
    add_requires("simdjson v3.9.3")
    add_packages("simdjson")
end

if has_config("enable_rapidjson") then
    add_requires("rapidjson")
    add_packages("rapidjson")
end

if has_config("enable_spdlog") then
    add_requires("spdlog")
    add_packages("spdlog")
end

if has_config("enable_fmt") then
    add_requires("fmt")
    add_packages("fmt")
end

if has_config("enable_communication") then
    add_requires("ilias")
end

includes("tests")

if is_mode("debug") then
    add_defines("NEKO_PROTO_LOG_CONTEXT")
end 

if is_plat("linux") then
    add_cxxflags("-fcoroutines")
end

target("NekoProtoBase")
    set_kind("static")
    add_includedirs(".")
    add_defines("NEKO_PROTO_STATIC")
    add_defines("ILIAS_COROUTINE_LIFETIME_CHECK")
    add_options("enable_spdlog", "enable_fmt", "enable_stdformat", "enable_rapidjson", "enable_simdjson")
    add_files("src/proto_base.cpp")
target_end()

if has_config("enable_communication") then
    target("NekoCommunicationBase")
        set_kind("static")
        add_includedirs(".")
        add_defines("NEKO_PROTO_STATIC")
        add_defines("ILIAS_COROUTINE_LIFETIME_CHECK")
        add_packages("ilias")
        set_languages("c++20")
        add_deps("NekoProtoBase")
        add_options("enable_spdlog", "enable_fmt", "enable_stdformat", "enable_rapidjson", "enable_simdjson")
        add_files("src/communication_base.cpp")
    target_end()
end 

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
