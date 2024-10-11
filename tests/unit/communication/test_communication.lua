if has_config("enable_communication") then
    target("test_communication")
        set_kind("binary")
        set_languages("c++20")
        set_default(false)
        add_includedirs("$(projectdir)/include")
        add_packages("ilias")
        add_defines("NEKO_PROTO_STATIC", "NEKO_COMMUNICATION_DEBUG")
        add_defines("ILIAS_COROUTINE_LIFETIME_CHECK")
        add_packages("gtest")
        add_options("enable_spdlog", "enable_fmt", "enable_stdformat", "enable_rapidjson", "enable_simdjson")
        add_tests("cpp20", {run_timeout = 5000})
        set_group("communication")
        add_files("$(projectdir)/src/communication_base.cpp")
        add_files("$(projectdir)/src/proto_base.cpp")
        add_files("test_communication.cpp")
        on_run(function (target)
            local argv = {}
            if has_config("memcheck") then
                table.insert(argv, "--leak-check=full")
                table.insert(argv, target:targetfile())
                os.execv("valgrind", argv)
            else
                os.run(target:targetfile())
            end
        end)
    target_end()
end