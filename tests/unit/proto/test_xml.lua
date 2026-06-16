if has_config("enable_pugixml") then
target("test_xml")
    set_kind("binary")
    set_default(false)
    add_defines("NEKO_PROTO_STATIC")
    add_includedirs("$(projectdir)/include")
    add_deps("NekoSerializer")
    add_tests("cpp23", {group = "proto", run_timeout = 5000, languages = "c++23"})
    if is_plat("linux") then
        add_ldflags("-lgmock", {force = true})
    end
    set_group("proto")
    add_files("test_xml.cpp")
    on_load(function (target)
        import("lua.auto", {rootdir = os.projectdir()})
        auto().auto_add_packages(target)
    end)
target_end()
end
