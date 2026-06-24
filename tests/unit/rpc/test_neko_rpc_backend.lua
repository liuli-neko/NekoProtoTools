if has_config("enable_jsonrpc") then
    target("test_neko_rpc_backend")
        set_kind("binary")
        set_default(false)
        add_includedirs("$(projectdir)/include")
        add_defines("NEKO_PROTO_STATIC")
        local cpp_versions = {stdcxx()}
        for i = 1, #cpp_versions do
            add_tests(cpp_versions[i]:gsub("%+", "p", 2), {
                group = "rpc",
                kind = "binary",
                files = {"../../../src/rpc.cpp", "test_neko_rpc_backend.cpp"},
                languages = cpp_versions[i],
                run_timeout = 30000
            })
        end
        set_group("rpc")
        add_files("test_neko_rpc_backend.cpp", "$(projectdir)/src/rpc.cpp")
        on_load(function (target)
            import("lua.auto", {rootdir = os.projectdir()})
            auto().auto_add_packages(target)
        end)
        if has_config("memcheck") then
            on_run(function (target)
                local argv = {}
                table.insert(argv, "--leak-check=full")
                table.insert(argv, target:targetfile())
                os.execv("valgrind", argv)
            end)
        end
    target_end()
end
