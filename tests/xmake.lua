if has_config("enable_tests") then

if has_config("ui_test") and has_config("enable_communication") and json_serializer_enabled() then
    add_requires("qtbase")
end

add_defines("ILIAS_ENABLE_LOG")

local function append_values(values, ...)
    for _, value in ipairs({...}) do
        table.insert(values, value)
    end
end

local function test_group(file)
    local group = path.filename(path.directory(file))
    if group == nil or group == "" or group == "." then
        return "base"
    end
    return group
end

local function default_test_config(file)
    local group = test_group(file)
    local config = {
        enabled = true,
        group = group,
        files = {file},
        deps = {},
        defines = {"NEKO_PROTO_STATIC"},
        run_timeout = 30000,
        use_gmock_tail_link = true
    }

    if group == "serializer" then
        config.deps = {"NekoSerializer"}
        config.run_timeout = 5000
    elseif group == "jsonrpc" then
        config.enabled = has_config("enable_jsonrpc") and json_serializer_enabled()
        config.deps = {"NekoJsonRpc", "NekoSerializer"}
        config.run_timeout = 5000
        -- table.insert(config.defines, "NEKO_VERBOSE_LOGS")
    elseif group == "rpc" then
        config.enabled = has_config("enable_jsonrpc") and json_serializer_enabled()
        config.deps = {"NekoJsonRpc", "NekoSerializer"}
    elseif group == "communication" then
        config.enabled = has_config("enable_communication") and json_serializer_enabled()
        config.deps = {"NekoCommunication"}
        config.run_timeout = 5000
    elseif group == "proto" then
        config.enabled = has_config("enable_protocol") and json_serializer_enabled()
        config.deps = {"NekoProtoBase", "NekoSerializer"}
    elseif group == "common" then
        config.deps = {"NekoSerializer"}
        config.run_timeout = 5000
    elseif group == "reflect" then
        config.deps = {"NekoSerializer"}
        config.run_timeout = 5000
    elseif group == "argparser" then 
        config.deps = {"NekoArgParser"}
    else
        append_values(config.files, "../src/proto_base.cpp", "../src/jsonrpc.cpp")
    end

    return config
end

local function add_memcheck_runner()
    if has_config("memcheck") then
        on_run(function (target)
            local argv = {}
            table.insert(argv, "--leak-check=full")
            table.insert(argv, target:targetfile())
            os.execv("valgrind", argv)
        end)
    end
end

local function define_test_target(file, config)
    local name = path.basename(file)
    target(name)
        set_kind("binary")
        set_default(false)
        add_includedirs("$(projectdir)/include")
        for _, define in ipairs(config.defines) do
            add_defines(define)
        end
        for _, dep in ipairs(config.deps) do
            add_deps(dep)
        end
        for _, source in ipairs(config.files) do
            add_files(source)
        end
        if is_plat("linux") and config.use_gmock_tail_link then
            add_ldflags("-lgmock", {force = true})
        end
        set_group(config.group)
        add_tests(stdcxx():gsub("%+", "p", 2),
                  {group = config.group,
                   kind = "binary",
                   languages = stdcxx(),
                   run_timeout = config.run_timeout})
        add_memcheck_runner()
        on_load(function (target)
            import("lua.auto", {rootdir = os.projectdir()})
            auto().auto_add_packages(target)
        end)
    target_end()
end

function neko_define_test(file, enabled)
    local config = default_test_config(file)
    if enabled and config.enabled then
        define_test_target(file, config)
    end
end

-- Make all files in the directory into targets
for _, file in ipairs(os.files("./**/test_*.cpp")) do
    local dir = path.directory(file)
    local name = path.basename(file)
    local conf_path = dir .. "/" .. name .. ".lua"

    -- If this file require a specific configuration, load it, and skip the auto target creation
    if os.exists(conf_path) then 
        includes(conf_path)
        goto continue
    end

    neko_define_test(file, true)

    ::continue::
end
end
