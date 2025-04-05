if has_config("enable_communication") then
    target("test_communication")
        set_kind("binary")
        set_languages("c++20")
        set_default(false)
        add_includedirs("$(projectdir)/include")
        add_packages("ilias")
        add_defines("NEKO_PROTO_STATIC", "ILIAS_USE_FMT", "ILIAS_ENABLE_LOG")
        add_packages("gtest")
        add_tests("cpp20", {run_timeout = 5000})
        set_group("communication")
        add_files("$(projectdir)/src/communication_base.cpp")
        add_files("$(projectdir)/src/proto_base.cpp")
        add_files("test_communication.cpp")
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