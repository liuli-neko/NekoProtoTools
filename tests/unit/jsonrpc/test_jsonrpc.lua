if has_config("enable_jsonrpc") then
    target("test_jsonrpc")
        set_kind("binary")
        set_languages("c++20")
        set_default(false)
        add_includedirs("$(projectdir)/include")
        add_packages("ilias", "gtest")
        add_defines("NEKO_PROTO_STATIC")
        add_tests("cpp20", {run_timeout = 5000})
        set_group("jsonrpc")
        add_files("test_jsonrpc.cpp")
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