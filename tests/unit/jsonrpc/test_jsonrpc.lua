if has_config("enable_jsonrpc") then
    target("test_jsonrpc")
        set_kind("binary")
        set_default(false)
        add_includedirs("$(projectdir)/include")
        add_defines("NEKO_PROTO_STATIC", "NEKO_VERBOSE_LOGS")
        local cpp_versions = {"c++20", "c++23"}
        for i = 1, #cpp_versions do
            add_tests(string.gsub(cpp_versions[i], '+', 'p', 2), {group = "jsonrpc", kind = "binary", files = {"test_jsonrpc.cpp"}, languages = cpp_versions[i], run_timeout = 30000})
        end
        set_group("jsonrpc")
        add_files("test_jsonrpc.cpp")
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