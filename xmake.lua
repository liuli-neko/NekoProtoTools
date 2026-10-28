set_project("neko-proto-tools")
add_rules("mode.debug", "mode.release", "mode.releasedbg", "mode.coverage", "mode.asan", "mode.ubsan", "mode.tsan")
includes("version.lua")
set_version(project_version())
add_repositories("btk-repo https://github.com/Btk-Project/xmake-repo.git")
set_warnings("allextra")
set_encodings("utf-8")
set_policy("package.cmake_generator.ninja", true)

option("stdc",   {showmenu = true, default = 23, values = {23}})
option("stdcxx", {showmenu = true, default = 20, values = {26, 23, 20}})
option("reflection_backend", {showmenu = true, default = "legacy", values = {"legacy", "native", "automatic"}})
option("binding_packs", {showmenu = true, default = "automatic", values = {"automatic", "on", "off"}})
option("pack_indexing", {showmenu = true, default = "automatic", values = {"automatic", "on", "off"}})
option("expansion_statements", {showmenu = true, default = false})
option("enable_modules", {showmenu = true, default = false, description = "Build named modules for all enabled components"})
if has_config("enable_modules") then
    set_policy("build.c++.modules", true)
    set_policy("build.c++.modules.std", false)
end
function stdc()   return "c"   .. tostring(get_config("stdc"))   end
function stdcxx() return "c++" .. tostring(get_config("stdcxx")) end
function json_serializer_enabled()
    return has_config("enable_yyjson") or has_config("enable_rapidjson") or has_config("enable_simdjson")
end

set_languages(stdc(), stdcxx())

option("has_native_reflection")
    set_showmenu(false)
    set_languages(stdcxx())
    add_cxxincludes("meta")
    add_cxxsnippets("native_reflection", [[
        #if !defined(__cpp_impl_reflection) || __cpp_impl_reflection < 202506L || !defined(__cpp_lib_reflection) || __cpp_lib_reflection < 202506L
        #error reflection unavailable
        #endif
        static_assert(std::meta::is_type(^^int));
    ]])
option_end()
option("has_native_reflection_flag")
    set_showmenu(false)
    set_languages(stdcxx())
    add_cxxflags("-freflection", {force = true})
    add_cxxincludes("meta")
    add_cxxsnippets("native_reflection_flag", [[
        #if !defined(__cpp_impl_reflection) || __cpp_impl_reflection < 202506L || !defined(__cpp_lib_reflection) || __cpp_lib_reflection < 202506L
        #error reflection unavailable
        #endif
        static_assert(std::meta::is_type(^^int));
    ]])
option_end()

option("has_binding_packs")
    set_showmenu(false)
    set_languages(stdcxx())
    add_cxxsnippets("has_binding_packs", [[
        #if !defined(__cpp_structured_bindings) || __cpp_structured_bindings < 202411L
        #error no binding packs
        #endif
    ]])
option_end()
option("has_pack_indexing")
    set_showmenu(false)
    set_languages(stdcxx())
    add_cxxsnippets("has_pack_indexing", [[
        #if !defined(__cpp_pack_indexing) || __cpp_pack_indexing < 202311L
        #error no pack indexing
        #endif
    ]])
option_end()

local reflection_backend = get_config("reflection_backend") or "legacy"
-- xmake reserves the literal "auto" for its own configuration machinery.
if reflection_backend == "automatic" then reflection_backend = "auto" end
local syntax_features = {binding_packs = "NEKO_USE_STRUCTURED_BINDING_PACKS", pack_indexing = "NEKO_USE_PACK_INDEXING"}
for option_name, macro in pairs(syntax_features) do
    local value = get_config(option_name) or "automatic"
    if value ~= "automatic" then add_defines(macro .. "=" .. (value == "on" and "1" or "0")) end
end
if has_config("expansion_statements") then add_defines("NEKO_USE_EXPANSION_STATEMENTS=1") end
add_defines("NEKO_PROTO_REFLECTION_BACKEND=" .. ({legacy = 0, native = 1, auto = 2})[reflection_backend])
if reflection_backend ~= "legacy" and not has_config("has_native_reflection") and has_config("has_native_reflection_flag") then
    add_cxxflags("-freflection", {force = true})
end

add_configfiles("include/nekoproto/global/config.h.in")
set_configdir("include/nekoproto/global")

includes("lua/hidetargets.lua")
includes("benchmarks/compile_time/xmake.lua")
includes("benchmarks/bench_json_task.lua")

option("has_std_expected")
    set_showmenu(false)
    set_languages(stdcxx())
    add_cxxincludes("version")
    add_cxxsnippets("has_std_expected", [[
        #if !defined(__cpp_lib_expected) || __cpp_lib_expected < 202202L
        #   error no std::expected support
        #endif
    ]])
option_end()

option("enable_yyjson")
    set_default(false)
    set_showmenu(true)
    set_description("Enable yyjson support, should install yyjson")
    set_category("serializer provider")
    set_configvar("NEKO_PROTO_ENABLE_YYJSON", true)
option_end()

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

option("enable_pugixml")
    set_default(false)
    set_showmenu(true)
    set_description("Enable pugixml support, should install pugixml")
    set_category("serializer provider")
    set_configvar("NEKO_PROTO_ENABLE_PUGIXML", true)
option_end()

option("enable_libfyaml")
    set_default(false)
    set_showmenu(true)
    set_description("Enable libfyaml YAML support, should install libfyaml")
    set_category("serializer provider")
    set_configvar("NEKO_PROTO_ENABLE_LIBFYAML", true)
option_end()

option("enable_yamlcpp")
    set_default(false)
    set_showmenu(true)
    set_description("Enable yaml-cpp YAML support, should install yaml-cpp")
    set_category("serializer provider")
    set_configvar("NEKO_PROTO_ENABLE_YAMLCPP", true)
option_end()

option("enable_tomlplusplus")
    set_default(false)
    set_showmenu(true)
    set_description("Enable toml++ TOML support, should install toml++")
    set_category("serializer provider")
    set_configvar("NEKO_PROTO_ENABLE_TOMLPLUSPLUS", true)
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

option("enable_benchmarks")
    set_default(false)
    set_showmenu(true)
    set_description("Enable JSON comparison benchmarks (Glaze requires C++23)")
    set_category("benchmark")
option_end()

option("enable_examples")
    set_default(false)
    set_showmenu(true)
    set_description("Build standalone examples and register their smoke tests")
    set_category("examples")
option_end()

-- Test sub-options must be declared at project scope. Declaring them only in
-- tests/xmake.lua makes them unavailable while `xmake f` is deciding whether
-- that file should be included, so a clean checkout cannot enable them.
option("fuzzer_test")
    set_default(false)
    set_showmenu(true)
    set_description("Enable libFuzzer targets (requires Clang/libFuzzer)")
    set_category("enable test")
option_end()

option("has_fuzzer_toolchain")
    set_showmenu(false)
    set_languages(stdcxx())
    add_cxxsnippets("has_fuzzer_toolchain", [[
        #if !defined(__clang__) || defined(_MSC_VER)
        #   error libFuzzer targets require a Clang-compatible toolchain
        #endif
    ]])
option_end()

option("ui_test")
    set_default(false)
    set_showmenu(true)
    set_description("Enable UI tests (requires Qt)")
    set_category("enable test")
option_end()

option("cereal_test")
    set_default(false)
    set_showmenu(true)
    set_description("Enable cereal comparison tests")
    set_category("enable test")
option_end()

if is_plat("linux") then
    option("memcheck")
        set_default(false)
        set_showmenu(true)
        set_description("Run unit targets through Valgrind")
        set_category("enable test")
    option_end()
end

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

option("3rd_custom",   {showmenu = true, type = "boolean", default = false})

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

option("enable_rpc_trace")
    set_default(false)
    set_showmenu(true)
    set_description("Enable RPC remote call tracing and WebUI monitoring console")
    set_category("modules")
    set_configvar("NEKO_PROTO_RPC_TRACE", true)
option_end()

option("use_io_uring")
    set_default(false)
    set_showmenu(true)
    set_description("Make ilias use io_uring backend")
    set_category("advanced")
option_end()


if has_config("enable_yyjson") then
    add_requires("yyjson", {configs = { shared = is_kind("shared")}})
end

if has_config("enable_simdjson") then
    add_requires("simdjson", {configs = { shared = false}})
end

if has_config("enable_rapidjson") then
    add_requires("rapidjson", {configs = { shared = is_kind("shared")}})
end

if has_config("enable_pugixml") then
    add_requires("pugixml", {configs = { shared = is_kind("shared")}})
end

if has_config("enable_libfyaml") then
    add_requires("libfyaml", {configs = { shared = is_kind("shared")}})
end

if has_config("enable_yamlcpp") then
    add_requires("yaml-cpp", {configs = { shared = is_kind("shared")}})
    if not has_config("3rd_custom") then
        add_requireconfs("**yaml-cpp", {version = "0.8.0"})
    end
end

if has_config("enable_tomlplusplus") then
    add_requires("toml++", {configs = { header_only = true}})
    if not has_config("3rd_custom") then
        add_requireconfs("**toml++", {version = "v3.4.0"})
    end
end

if has_config("enable_spdlog") then
    add_requires("spdlog", {configs = { shared = is_kind("shared")}})
end

if has_config("enable_fmt") then
    add_requires("fmt", {configs = { shared = is_kind("shared")}})
end

if not has_config("has_std_expected") then
    add_requires("zeus_expected")
    if not has_config("3rd_custom") then
        add_requireconfs("**zeus_expected", {version = "v1.4.0"})
    end
end

if has_config("enable_communication") or
   (has_config("enable_jsonrpc") and json_serializer_enabled()) then
    if has_config("3rd_custom") then
        add_requires("ilias", {configs = {modules = has_config("enable_modules"), stdcxx = tonumber(get_config("stdcxx"))}})
    else
        -- Pin the direct dependency too: add_requireconfs only affects transitive requirements.
        add_requires("ilias 0.5.1", {configs = {shared = is_kind("shared"), stdcxx = tonumber(get_config("stdcxx")), modules = has_config("enable_modules")}})
    end
end

if has_config("enable_tests") then
    add_requires("gtest", "cpptrace")
    includes("tests")
end

if has_config("enable_benchmarks") then
    includes("benchmarks/xmake.lua")
end

if has_config("enable_examples") then
    includes("examples/xmake.lua")
end

if is_mode("debug") or is_mode("asan") or is_mode("ubsan") or is_mode("tsan") then
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

local function reflection_headers()
    add_headerfiles("include/(nekoproto/reflect.hpp)")
    add_headerfiles("include/(nekoproto/reflection/**.hpp)")
    add_headerfiles("include/(nekoproto/modules/**.hpp)")
    add_headerfiles("include/(nekoproto/global/**.hpp)")
    add_headerfiles("include/(nekoproto/global/**.h)")
    add_headerfiles("include/(nekoproto/serialization/reflection.hpp)", "include/(nekoproto/serialization/tags.hpp)")
end

local function reflection_settings()
    add_defines("NEKO_PROTO_REFLECTION_BACKEND=" .. ({legacy = 0, native = 1, auto = 2})[reflection_backend], {public = true})
    for option_name, macro in pairs(syntax_features) do
        local value = get_config(option_name) or "automatic"
        if value ~= "automatic" then add_defines(macro .. "=" .. (value == "on" and "1" or "0"), {public = true}) end
    end
    if has_config("expansion_statements") then add_defines("NEKO_USE_EXPANSION_STATEMENTS=1", {public = true}) end
    if reflection_backend ~= "legacy" and not has_config("has_native_reflection") and has_config("has_native_reflection_flag") then
        add_cxxflags("-freflection", {public = true, force = true})
    end
end

local module_headers_hash
local function module_settings(target, bytes)
    -- xmake's BMI cache currently watches module sources and flags, not their
    -- textual includes. Keep shared headers/configuration in that cache key.
    if not module_headers_hash then
        local headers = os.files(path.join(os.projectdir(), "include/nekoproto/**.hpp"))
        table.join2(headers, os.files(path.join(os.projectdir(), "include/nekoproto/**.h")))
        table.sort(headers)
        local hashes = {}
        for _, header in ipairs(headers) do table.insert(hashes, hash.sha256(header)) end
        module_headers_hash = hash.sha256(bytes(table.concat(hashes, ":"))):sub(1, 16)
    end
    target:add("defines", "NEKO_MODULE_HEADERS_" .. module_headers_hash, {public = true})
    local function selected(option_name, detected)
        local value = get_config(option_name) or "automatic"
        return value == "on" or (value == "automatic" and has_config(detected))
    end
    local native = reflection_backend ~= "legacy" and
                   (has_config("has_native_reflection") or has_config("has_native_reflection_flag"))
    target:add("defines", "NEKO_USE_NATIVE_REFLECTION=" .. (native and "1" or "0"), {public = true})
    target:add("defines", "NEKO_USE_STRUCTURED_BINDING_PACKS=" .. (selected("binding_packs", "has_binding_packs") and "1" or "0"), {public = true})
    target:add("defines", "NEKO_USE_PACK_INDEXING=" .. (selected("pack_indexing", "has_pack_indexing") and "1" or "0"), {public = true})
    target:add("defines", "NEKO_USE_EXPANSION_STATEMENTS=" .. (has_config("expansion_statements") and "1" or "0"), {public = true})
end

local function reflection_log_packages(target)
    if has_config("enable_fmt") then target:add("packages", "fmt", {public = true}) end
    if has_config("enable_spdlog") then target:add("packages", "spdlog", {public = true}) end
end

target("NekoReflection")
    set_kind("headeronly")
    reflection_headers()
    reflection_settings()
    add_options("enable_fmt", "enable_stdformat", "enable_spdlog", "has_binding_packs", "has_pack_indexing")
    add_includedirs("include", {public = true})
    on_load(function (target)
        reflection_log_packages(target)
        if has_config("enable_modules") then
            import("core.base.bytes")
            module_settings(target, bytes)
        end
        if reflection_backend == "native" then
            assert(tonumber(get_config("stdcxx")) == 26, "native reflection requires --stdcxx=26")
            assert(has_config("has_native_reflection") or has_config("has_native_reflection_flag"), "native reflection feature probe failed")
        end
    end)
target_end()

target("NekoSerializer")
    set_kind("headeronly")
    add_deps("NekoReflection", {public = true})
    add_headerfiles("include/(nekoproto/global/**.hpp)")
    add_headerfiles("include/(nekoproto/global/**.h)")
    add_headerfiles("include/(nekoproto/serialization/**.hpp)")
    add_headerfiles("include/(nekoproto/serializer.hpp)", "include/(nekoproto/serialization.hpp)")
    add_includedirs("include")

    add_options("enable_spdlog", 
                "enable_fmt", 
                "enable_stdformat", 
                "enable_yyjson",
                "enable_rapidjson", 
                "enable_simdjson", 
                "enable_pugixml",
                "enable_libfyaml",
                "enable_yamlcpp",
                "enable_tomlplusplus",
                "enable_protocol",
                "enable_communication",
                "enable_jsonrpc",
                "enable_rpc_trace")
    on_load(function (target)
        import("lua.auto", {rootdir = os.projectdir()})
        auto().auto_add_packages(target, {uses_expected = true})
    end)
target_end()

target("NekoArgParser")
    set_kind("headeronly")
    add_deps("NekoReflection", {public = true})
    add_headerfiles("include/(nekoproto/global/**.hpp)")
    add_headerfiles("include/(nekoproto/global/**.h)")
    add_headerfiles("include/(nekoproto/serialization/reflection.hpp)")
    add_headerfiles("include/(nekoproto/serialization/private/**.hpp)")
    add_headerfiles("include/(nekoproto/argparser/**.hpp)")
    add_includedirs("include")

    add_options("enable_spdlog",
                "enable_fmt",
                "enable_stdformat",
                "enable_yyjson",
                "enable_rapidjson",
                "enable_simdjson",
                "enable_libfyaml",
                "enable_yamlcpp",
                "enable_tomlplusplus")
    on_load(function (target)
        import("lua.auto", {rootdir = os.projectdir()})
        auto().auto_add_packages(target, {uses_expected = true})
    end)
target_end()

if has_config("enable_jsonrpc") and json_serializer_enabled() then
    target("NekoJsonRpc")
        -- TODO: 考虑改为 shared 或 static 以进一步减少编译时间
        -- 当前保持 headeronly + 部分实现分离到 cpp 的方式
        if is_kind("shared") then
            set_kind("shared")
            add_defines("NEKO_PROTO_LIBRARY")
        else
            set_kind("static")
            set_configvar("NEKO_PROTO_STATIC", true)
        end
        add_headerfiles("include/(nekoproto/jsonrpc/**.hpp)")
        add_headerfiles("include/(nekoproto/rpc/**.hpp)")
        add_headerfiles("include/(nekoproto/transport/**.hpp)")
        add_includedirs("include")
        -- 添加非模板函数的实现文件
        add_files("src/jsonrpc.cpp", "src/rpc.cpp")
        if has_config("enable_rpc_trace") then
            add_files("src/rpc_tracing.cpp")
        end
        add_options("enable_rpc_trace")

        on_load(function (target) 
            import("lua.auto", {rootdir = os.projectdir()})
            auto().target_autoclean(target)
            auto().auto_add_packages(target, {uses_ilias = true})
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

        on_load(function (target) 
            import("lua.auto", {rootdir = os.projectdir()})
            auto().target_autoclean(target)
            auto().auto_add_packages(target)
        end)
    target_end()
end

if has_config("enable_communication") then
    target("NekoCommunication")
        if is_kind("shared") then
            set_kind("shared")
            add_defines("NEKO_PROTO_LIBRARY")
        else
            set_kind("static")
            set_configvar("NEKO_PROTO_STATIC", true)
        end

        add_deps("NekoProtoBase", "NekoSerializer")
        add_headerfiles("include/(nekoproto/communication/**.hpp)")
        add_headerfiles("include/(nekoproto/transport/**.hpp)")
        add_includedirs("include")
        add_files("src/communication_base.cpp")

        on_load(function (target)
            import("lua.auto", {rootdir = os.projectdir()})
            auto().target_autoclean(target)
            auto().auto_add_packages(target, {uses_ilias = true})
        end)
    target_end()
end

-- Module publication follows component enablement; headers remain installed.
if has_config("enable_modules") then
    if has_config("enable_communication") or (has_config("enable_jsonrpc") and json_serializer_enabled()) then
        target("NekoIliasModule")
            set_kind("static")
            add_packages("ilias", {public = true})
            add_files("modules/ilias.cpp")
        target_end()
    end
    local function component_module(component, name, deps, uses_ilias)
        target("Neko" .. component .. "Module")
            set_kind("static")
            set_policy("build.c++.modules", true)
            add_files("modules/nekoproto." .. name .. ".cppm", {public = true})
            for _, dep in ipairs(deps) do add_deps(dep, {public = true}) end
            add_headerfiles("include/(nekoproto/**.hpp)", "include/(nekoproto/**.h)")
            add_includedirs("include", {public = true})
            on_load(function (target)
                import("lua.auto", {rootdir = os.projectdir()})
                import("core.base.bytes")
                auto().auto_add_packages(target, {uses_expected = true, uses_ilias = uses_ilias})
                module_settings(target, bytes)
            end)
        target_end()
    end
    component_module("Reflection", "reflect", {"NekoReflection"})
    component_module("Serializer", "serializer", {"NekoSerializer", "NekoReflectionModule"})
    component_module("ArgParser", "argparser", {"NekoArgParser", "NekoSerializerModule"})
    if has_config("enable_protocol") then
        component_module("Protocol", "proto", {"NekoProtoBase", "NekoSerializerModule"})
    end
    if has_config("enable_communication") or (has_config("enable_jsonrpc") and json_serializer_enabled()) then
        component_module("Transport", "transport", {"NekoSerializerModule"}, true)
    end
    if has_config("enable_jsonrpc") and json_serializer_enabled() then
        component_module("Rpc", "rpc", {"NekoJsonRpc", "NekoTransportModule"}, true)
        component_module("JsonRpc", "jsonrpc", {"NekoJsonRpc", "NekoRpcModule"}, true)
    end
    if has_config("enable_communication") then
        component_module("Communication", "communication", {"NekoCommunication", "NekoProtocolModule", "NekoTransportModule"}, true)
    end
end
