if has_config("enable_protocol") then
target("test_serializer_in_main")
    set_kind("binary")
    set_default(false)
    add_includedirs("$(projectdir)/include")
    add_deps("NekoProtoBase")
    add_defines("NEKO_VERBOSE_LOGS")
    add_tests("cpp20", {run_timeout = 5000, languages = "c++20"})
    set_group("proto")
    add_files("test_serializer_in_main.cpp")
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