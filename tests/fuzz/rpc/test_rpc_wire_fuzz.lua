if has_config("fuzzer_test") and has_config("has_fuzzer_toolchain") and has_config("enable_jsonrpc") and
   (has_config("enable_rapidjson") or has_config("enable_simdjson")) then
    target("test_rpc_wire_fuzz")
        set_kind("binary")
        set_default(false)
        set_languages(stdcxx())
        add_defines("NEKO_PROTO_STATIC")
        add_deps("NekoJsonRpc", "NekoSerializer")
        add_includedirs("$(projectdir)/include")
        add_cxxflags("-fsanitize=fuzzer,address,undefined", {force = true})
        add_ldflags("-fsanitize=fuzzer,address,undefined", {force = true})
        add_files("test_rpc_wire_fuzz.cpp")
        on_load(function (target)
            import("lua.auto", {rootdir = os.projectdir()})
            auto().auto_add_packages(target)
        end)
    target_end()
end
