set_project("neko-proto-tools")
add_rules("mode.debug", "mode.release", "mode.releasedbg", "mode.coverage", "mode.asan")
set_version("0.2.6", {build = "%Y%m%d%H%M"})
add_repositories("btk-repo https://github.com/Btk-Project/xmake-repo.git")
set_warnings("allextra")
set_policy("package.cmake_generator.ninja", true)
set_languages("c++20")

add_configfiles("include/nekoproto/global/config.h.in")
set_configdir("include/nekoproto/global")

includes("lua/hidetargets.lua")

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

option("enable_protocol")
    set_default(true)
    set_showmenu(true)
    set_description("Enable protocol module")
    set_category("modules")
option_end()

option("enable_communication")
    set_default(false)
    set_showmenu(true)
    add_deps("enable_protocol")
    set_description("Enable communication module, need ilias and protocol module")
    set_category("modules")
    after_check(function (option)
        if option:enabled() then
            assert(option:dep("enable_protocol"):enabled(), "enable_protocol must be enabled when enable_communication is enabled")
        end
    end)
option_end()

option("enable_jsonrpc")
    set_default(false)
    set_showmenu(true)
    set_description("Enable jsonrpc support, need ilias module")
    set_category("modules")
option_end()

option("custom_namespace")
    set_default("NekoProto")
    set_showmenu(true)
    set_description("Custom namespace for generated code")
    set_category("advanced")
option_end()

option("use_io_uring")
    set_default(false)
    set_showmenu(true)
    set_description("Make ilias use io_uring backend")
    set_category("advanced")
option_end()


if has_config("enable_simdjson") then
    add_requires("simdjson v3.9.3", {configs = { shared = is_kind("shared")}})
end

if has_config("enable_rapidjson") then
    add_requires("rapidjson", {configs = { shared = is_kind("shared")}})
end

if has_config("enable_rapidxml") then
    add_requires("rapidxml", {configs = { shared = is_kind("shared")}})
end

if has_config("enable_spdlog") then
    add_requires("spdlog", {configs = { shared = is_kind("shared")}})
end

if has_config("enable_fmt") then
    add_requires("fmt", {configs = { shared = is_kind("shared")}})
end

if has_config("enable_communication") or has_config("enable_jsonrpc") then
    add_requires("ilias", {version = "0.3.3", configs = { shared = is_kind("shared"), cpp20 = true}})
    add_requireconfs("**.ilias", {version = "0.3.3", configs = { shared = is_kind("shared"), cpp20 = true}})
end

if has_config("enable_tests") then
    add_requires("gtest", "cpptrace")
    includes("tests")
end

if is_mode("debug") or is_mode("asan") then
    add_defines("NEKO_PROTO_LOG_CONTEXT")
    if is_plat("linux") then
        add_cxxflags("-ftemplate-backtrace-limit=0")
    end
end 

if is_plat("linux") then
    add_cxxflags("-fcoroutines")
end

if is_plat("windows") then 
    add_cxxflags("/bigobj", "/Zc:preprocessor")
end

target("NekoSerializer")
    set_kind("headeronly")
    add_headerfiles("include/(nekoproto/global/**.hpp)")
    add_headerfiles("include/(nekoproto/global/**.h)")
    add_headerfiles("include/(nekoproto/serialization/**.hpp)")
    add_includedirs("include")

    add_options("enable_spdlog", 
                "enable_fmt", 
                "enable_stdformat", 
                "enable_rapidjson", 
                "enable_simdjson", 
                "enable_rapidxml", 
                "enable_protocol",
                "enable_communication",
                "enable_jsonrpc",
                "custom_namespace")

    set_configvar("NEKO_NAMESPACE", "$(custom_namespace)")
    on_load(function (target)
        import("lua.auto", {rootdir = os.projectdir()})
        auto().auto_add_packages(target)
    end)
target_end()

if has_config("enable_jsonrpc") then
    target("NekoJsonRpc")
        set_kind("headeronly")
        add_headerfiles("include/(nekoproto/jsonrpc/**.hpp)")
        add_includedirs("include")

        on_load(function (target) 
            import("lua.auto", {rootdir = os.projectdir()})
            auto().auto_add_packages(target)
        end)
    target_end()
end

if has_config("enable_protocol") then
    target("NekoProtoBase")
        if is_kind("shared") then
            set_kind("shared")
            add_defines("NEKO_PROTO_LIBRARY")
        else
            set_kind("static")
            set_configvar("NEKO_PROTO_STATIC", true)
        end
        
        add_headerfiles("include/(nekoproto/proto/**.hpp)")
        add_includedirs("include")
        add_files("src/proto_base.cpp")
        if has_config("enable_communication") then
            add_headerfiles("include/(nekoproto/communication/**.hpp)")
            add_files("src/communication_base.cpp")
        end

        on_load(function (target) 
            import("lua.auto", {rootdir = os.projectdir()})
            auto().auto_add_packages(target)
        end)
    target_end()
end
