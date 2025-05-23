if has_config("fuzzer_test") then
    target("test_json_proto")
        set_kind("binary")
        set_default(false)
        set_languages("c++17")
        add_defines("NEKO_PROTO_STATIC")
        add_packages("gtest")
        set_policy("build.sanitizer.address", true)
        add_includedirs("../../../")
        set_toolchains("clang-14")
        add_cxxflags("clang::-fsanitize=fuzzer")
        add_linkdirs("/usr/lib/llvm-14/lib")
        add_links("Fuzzer")
        add_files("../../../src/proto_base.cpp")
        add_files("test_json_proto.cpp")
        on_load(function (target) 
            import("lua.auto", {rootdir = os.projectdir()})
            auto().auto_add_packages(target)
        end)
    target_end()
end 