if has_config("enable_communication") then
    target("test_communication")
        set_kind("binary")
        set_default(false)
        add_includedirs("$(projectdir)/include")
        add_defines("NEKO_PROTO_STATIC")
        add_tests("cpp20", {run_timeout = 5000, languages = "c++20"})
        set_group("communication")
        add_files("$(projectdir)/src/communication_base.cpp")
        add_files("$(projectdir)/src/proto_base.cpp")
        add_files("test_communication.cpp")
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