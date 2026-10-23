if not has_config("enable_modules") then return end
local function module_test(name, sources, deps, defines)
    target(name)
        set_kind("binary")
        set_default(false)
        set_group("modules")
        set_policy("build.c++.modules", true)
        add_files(table.unpack(sources))
        add_deps(table.unpack(deps))
        if defines then add_defines(table.unpack(defines)) end
        add_tests("regression")
    target_end()
end
local native = get_config("reflection_backend") ~= "legacy" and
               (has_config("has_native_reflection") or has_config("has_native_reflection_flag"))
module_test("module_consumer", {"module_main.cpp", "module_header.cpp"}, {"NekoReflectionModule"}, native and {"NEKO_TEST_NATIVE"} or {})
module_test("module_pure", {"module_pure.cpp"}, {"NekoReflectionModule"})
module_test("module_type_detection", {"../cpp26/type_detection.cpp"}, {"NekoReflectionModule"}, {"NEKO_TEST_TYPE_DETECTION_MODULE"})
local components = {"Serializer", "ArgParser"}
if has_config("enable_protocol") then table.insert(components, "Protocol") end
if has_config("enable_communication") or (has_config("enable_jsonrpc") and json_serializer_enabled()) then
    table.insert(components, "Transport")
end
if has_config("enable_jsonrpc") and json_serializer_enabled() then
    table.insert(components, "Rpc")
    table.insert(components, "JsonRpc")
end
if has_config("enable_communication") then table.insert(components, "Communication") end
local all_deps, all_defines = {}, {}
for _, component in ipairs(components) do
    local dep, define = "Neko" .. component .. "Module", "NEKO_TEST_" .. component:upper()
    module_test("module_" .. component, {"component.cpp", "component_header.cpp"}, {dep}, {define})
    table.insert(all_deps, dep)
    table.insert(all_defines, define)
end
module_test("module_all", {"component.cpp", "component_header.cpp"}, all_deps, all_defines)
