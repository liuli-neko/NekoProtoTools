if has_config("cereal_test") then
    add_requires("cereal")
    target("test_cbproto")
        set_kind("binary")
        add_includedirs("$(projectdir)/include")
        set_languages("c++17")
        add_packages("gtest", "cereal")
        add_options("enable_spdlog", "enable_fmt", "enable_stdformat", "enable_rapidjson", "enable_simdjson")
        add_files("test_random_big_data_cereal.cpp")
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