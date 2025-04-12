if has_config("cereal_test") then
    add_requires("cereal")
    target("test_cbproto")
        set_kind("binary")
        add_includedirs("$(projectdir)/include")
        add_files("test_random_big_data_cereal.cpp")
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