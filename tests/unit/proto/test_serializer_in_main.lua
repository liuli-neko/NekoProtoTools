target("test_serializer_in_main")
    set_kind("binary")
    set_languages("c++20")
    set_default(false)
    add_includedirs("$(projectdir)/include")
    add_deps("NekoProtoBase")
    add_defines("NEKO_VERBOSE_LOGS", "SIMDJSON_EXCEPTIONS=1")
    add_packages("gtest")
    add_options("enable_spdlog", "enable_fmt", "enable_stdformat", "enable_rapidjson", "enable_simdjson")
    add_tests("serializer_in_main")
    set_group("proto")
    add_files("test_serializer_in_main.cpp")
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