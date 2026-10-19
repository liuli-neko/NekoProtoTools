set_project("installed-module-consumer")
add_repositories("btk-repo https://github.com/Btk-Project/xmake-repo.git")
option("stdcxx", {default = 26, values = {20, 23, 26}, showmenu = true})
set_languages("c++" .. tostring(get_config("stdcxx") or 26))
add_rules("mode.release", "mode.debug")
set_policy("check.auto_ignore_flags", false)
set_policy("build.c++.modules.std", false)
option("neko_prefix", {type = "string", showmenu = true})
option("ilias_tls", {default = true, showmenu = true})
option("has_std_expected")
    set_showmenu(false)
    set_languages("c++" .. tostring(get_config("stdcxx") or 26))
    add_cxxincludes("version")
    add_cxxsnippets("std_expected", [[
        #if !defined(__cpp_lib_expected) || __cpp_lib_expected < 202202L
        #error no std::expected
        #endif
    ]])
option_end()
local prefix = get_config("neko_prefix")
local function installed_module(name)
    return prefix and #os.files(path.join(prefix, "modules", "*", "nekoproto." .. name .. ".cppm.meta-info")) > 0
end
if installed_module("transport") then
    -- xmake's module scanner needs the module package on the target itself.
    add_requires("ilias 0.5.1", {configs = {stdcxx = tonumber(get_config("stdcxx")), modules = true,
                                         shared = is_kind("shared"), tls = has_config("ilias_tls")}})
end
if prefix then
    package("neko_installed")
        set_installdir(prefix)
        on_load(function (package)
            local config = io.readfile(path.join(package:installdir(), "include/nekoproto/global/config.h"))
            local function enabled(macro)
                return config:find("#define%s+" .. macro .. "%f[%W]") ~= nil
            end
            if enabled("NEKO_PROTO_USE_FMT") then package:add("deps", "fmt") end
            if enabled("NEKO_PROTO_USE_SPDLOG") then package:add("deps", "spdlog") end
            if enabled("NEKO_PROTO_ENABLE_RAPIDJSON") then package:add("deps", "rapidjson 2025.02.05") end
            if enabled("NEKO_PROTO_ENABLE_YYJSON") then package:add("deps", "yyjson 0.12.0") end
            if enabled("NEKO_PROTO_ENABLE_SIMDJSON") then package:add("deps", "simdjson v4.6.11") end
            if enabled("NEKO_PROTO_ENABLE_PUGIXML") then package:add("deps", "pugixml v1.16") end
            if enabled("NEKO_PROTO_ENABLE_LIBFYAML") then package:add("deps", "libfyaml v0.9.5") end
            if enabled("NEKO_PROTO_ENABLE_YAMLCPP") then package:add("deps", "yaml-cpp 0.8.0") end
            if enabled("NEKO_PROTO_ENABLE_TOMLPLUSPLUS") then package:add("deps", "toml++ v3.4.0", {configs = {header_only = true}}) end
            if not has_config("has_std_expected") then
                package:add("deps", "zeus_expected v1.4.0")
            end
            if installed_module("transport") then
                package:add("deps", "ilias 0.5.1", {configs = {stdcxx = tonumber(get_config("stdcxx")), modules = true,
                                      shared = not enabled("NEKO_PROTO_STATIC"), tls = has_config("ilias_tls")}})
            end
        end)
        on_fetch(function (package)
            import("core.base.json")
            local root = package:installdir()
            local files = os.files(path.join(root, "modules", "*", "nekoproto.reflect.cppm.meta-info"))
            local metadata = json.loadfile(assert(files[1], "installed module metadata missing"))
            local links = {}
            -- Static archives are ordered from dependents to dependencies.
            for _, name in ipairs({"JsonRpcModule", "RpcModule", "CommunicationModule", "ProtocolModule", "ArgParserModule", "TransportModule", "SerializerModule", "ReflectionModule", "Communication", "JsonRpc", "ProtoBase", "IliasModule"}) do
                if os.isfile(path.join(root, "lib", "libNeko" .. name .. ".a")) or os.isfile(path.join(root, "lib", "libNeko" .. name .. ".so")) then
                    table.insert(links, "Neko" .. name)
                end
            end
            local native = table.contains(metadata.defines, "NEKO_USE_NATIVE_REFLECTION=1")
            return {includedirs = path.join(root, "include"), linkdirs = path.join(root, "lib"),
                    links = links, defines = metadata.defines, cxxflags = native and {"-freflection"} or {}}
        end)
    package_end()
    add_requires("neko_installed", {system = false})
end
local function consumer(name, sources, defines)
    target(name)
        set_kind("binary")
        set_policy("build.c++.modules", true)
        add_packages("neko_installed")
        if installed_module("transport") then add_packages("ilias") end
        add_files(table.unpack(sources))
        if defines then add_defines(table.unpack(defines)) end
        add_tests("installed")
        on_load(function (target)
            assert(prefix, "pass --neko_prefix=<module install prefix>")
            import("core.base.json")
            local files = os.files(path.join(prefix, "modules", "*", "nekoproto.reflect.cppm.meta-info"))
            if table.contains(json.loadfile(files[1]).defines, "NEKO_USE_NATIVE_REFLECTION=1") then
                target:add("defines", "NEKO_TEST_NATIVE")
            end
        end)
    target_end()
end
consumer("installed_module_consumer", {"../../modules/module_main.cpp", "../../modules/module_header.cpp"})
consumer("installed_module_pure", {"../../modules/module_pure.cpp"})
for _, component in ipairs({{"Serializer", "serializer"}, {"ArgParser", "argparser"}, {"Protocol", "proto"}, {"Transport", "transport"}, {"Rpc", "rpc"}, {"JsonRpc", "jsonrpc"}, {"Communication", "communication"}}) do
    if installed_module(component[2]) then
        consumer("installed_module_" .. component[1], {"../../modules/component.cpp", "../../modules/component_header.cpp"}, {"NEKO_TEST_" .. component[1]:upper()})
    end
end
