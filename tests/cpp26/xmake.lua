local function reflection_test(name, source, defines, fails)
    target(name)
        set_kind("binary")
        set_default(false)
        set_group("reflection")
        add_deps("NekoSerializer")
        add_files(source)
        if defines then add_defines(table.unpack(defines)) end
        add_tests("regression", {build_should_fail = fails})
    target_end()
end
reflection_test("reflection_public_api", "public_api.cpp")
if tonumber(get_config("stdcxx")) == 26 then
    for _, count in ipairs({64, 65, 128, 129}) do
        if count ~= 129 or get_config("reflection_backend") == "native" or
           (get_config("binding_packs") ~= "off" and has_config("has_binding_packs")) then
            reflection_test("reflection_boundary_" .. count, "boundaries.cpp", {"NEKO_TEST_FIELD_COUNT=" .. count})
        end
    end
    reflection_test("reflection_generated_boundary", "boundaries.cpp", {"NEKO_TEST_FIELD_COUNT=128", "NEKO_TEST_GENERATED_BINDING"})
end
if get_config("reflection_backend") == "automatic" and (has_config("has_native_reflection") or has_config("has_native_reflection_flag")) then
    reflection_test("reflection_auto_backend", "auto_backend.cpp")
end
if get_config("reflection_backend") == "native" then
    reflection_test("reflection_native_backend", "native_backend.cpp")
    reflection_test("reflection_native_enum", "native_enum.cpp")
    if has_config("enable_rapidjson") and has_config("enable_pugixml") and
       (has_config("enable_libfyaml") or has_config("enable_yamlcpp")) and has_config("enable_tomlplusplus") then
        reflection_test("reflection_native_integration", "native_integration.cpp")
    end
    reflection_test("reflection_invalid_annotation", "invalid_annotation.cpp", nil, true)
    reflection_test("reflection_invalid_metadata", "invalid_metadata.cpp", nil, true)
end
