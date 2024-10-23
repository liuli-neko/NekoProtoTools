if has_config("cereal_test") then
    add_requires("cereal")
    target("test_cbproto")
        set_kind("binary")
        add_includedirs("$(projectdir)/include")
        set_languages("c++17")
        add_packages("gtest", "cereal")
        add_files("test_random_big_data_cereal.cpp")
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