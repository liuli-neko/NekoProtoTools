set_project("cc-proto")
add_rules("mode.debug", "mode.release", "mode.valgrind", "mode.coverage")
set_version("1.0.0")

add_rules("plugin.compile_commands.autoupdate", {outputdir = ".vscode"})

add_requires("rapidjson", "spdlog", "gtest")

if is_mode("debug") then
    add_defines("NEKO_PROTO_LOG_CONTEXT")
end 

if is_plat("linux") then
    add_cxxflags("-fcoroutines")
end

target("test_co")
    set_languages("c++20")
    set_kind("binary")
    add_packages("spdlog")
    add_tests("co")
    add_files("tests/test_co.cpp")
target_end()

target("NekoProtoBase")
    set_kind("static")
    add_defines("NEKO_PROTO_STATIC")
    add_defines("ILIAS_COROUTINE_LIFETIME_CHECK")
    add_packages("spdlog")
    add_files("src/proto_base.cpp")
target_end()

target("NekoCommunicationBase")
    set_kind("static")
    add_defines("NEKO_PROTO_STATIC")
    add_defines("ILIAS_COROUTINE_LIFETIME_CHECK")
    add_includedirs("./modules/Ilias/include")
    set_languages("c++20")
    add_deps("NekoProtoBase")
    add_packages("spdlog")
    add_files("src/communication_base.cpp")
target_end()

target("tests")
    set_kind("phony")
    -- js
    add_tests("js_cpp14", {group = "js", kind = "binary", defines = "NEKO_PROTO_STATIC", packages = {"rapidjson", "spdlog", "gtest"}, files = {"tests/test_json_serializer.cpp", "src/proto_base.cpp"}, languages = "c++14", run_timeout = 1000})
    add_tests("js_cpp17", {group = "js", kind = "binary", defines = "NEKO_PROTO_STATIC", packages = {"rapidjson", "spdlog", "gtest"}, files = {"tests/test_json_serializer.cpp", "src/proto_base.cpp"}, languages = "c++17", run_timeout = 1000})
    add_tests("js_cpp20", {group = "js", kind = "binary", defines = "NEKO_PROTO_STATIC", packages = {"rapidjson", "spdlog", "gtest"}, files = {"tests/test_json_serializer.cpp", "src/proto_base.cpp"}, languages = "c++20", run_timeout = 1000})

    -- proto
    -- add_tests("proto_cpp11", {group = "proto", kind = "binary", defines = "NEKO_PROTO_STATIC", packages = {"rapidjson", "spdlog", "gtest"}, files = {"tests/test_proto.cpp", "src/proto_base.cpp"}, languages = "c++11", run_timeout = 1000})
    add_tests("proto_cpp14", {group = "proto", kind = "binary", defines = "NEKO_PROTO_STATIC", packages = {"rapidjson", "spdlog", "gtest"}, files = {"tests/test_proto.cpp", "src/proto_base.cpp"}, languages = "c++14", run_timeout = 1000})
    add_tests("proto_cpp17", {group = "proto", kind = "binary", defines = "NEKO_PROTO_STATIC", packages = {"rapidjson", "spdlog", "gtest"}, files = {"tests/test_proto.cpp", "src/proto_base.cpp"}, languages = "c++17", run_timeout = 1000})
    add_tests("proto_cpp20", {group = "proto", kind = "binary", defines = "NEKO_PROTO_STATIC", packages = {"rapidjson", "spdlog", "gtest"}, files = {"tests/test_proto.cpp", "src/proto_base.cpp"}, languages = "c++20", run_timeout = 1000})

    -- communication
    -- add_tests("communication_cpp20", {group = "communication", kind = "binary", defines = {"NEKO_PROTO_STATIC", "ILIAS_COROUTINE_LIFETIME_CHECK"}, includedirs = {"./modules/Ilias/include"}, deps = "NekoCommunicationBase", packages = {"rapidjson", "spdlog", "gtest"}, files = {"tests/test_communication.cpp"}, languages = "c++20", run_timeout = 1000})

    -- binary serializer
    -- add_tests("bs_cpp11", {group = "bs", kind = "binary", defines = "NEKO_PROTO_STATIC", packages = {"rapidjson", "spdlog", "gtest"}, files = {"tests/test_binary_serializer.cpp", "src/proto_base.cpp"}, languages = "c++11", run_timeout = 1000})
    add_tests("bs_cpp14", {group = "bs", kind = "binary", defines = "NEKO_PROTO_STATIC", packages = {"rapidjson", "spdlog", "gtest"}, files = {"tests/test_binary_serializer.cpp", "src/proto_base.cpp"}, languages = "c++14", run_timeout = 1000})
    add_tests("bs_cpp17", {group = "bs", kind = "binary", defines = "NEKO_PROTO_STATIC", packages = {"rapidjson", "spdlog", "gtest"}, files = {"tests/test_binary_serializer.cpp", "src/proto_base.cpp"}, languages = "c++17", run_timeout = 1000})
    add_tests("bs_cpp20", {group = "bs", kind = "binary", defines = "NEKO_PROTO_STATIC", packages = {"rapidjson", "spdlog", "gtest"}, files = {"tests/test_binary_serializer.cpp", "src/proto_base.cpp"}, languages = "c++20", run_timeout = 1000})
    add_defines("NEKO_PROTO_STATIC")
    -- private invoid params
    add_tests("private_cpp14", {group = "p", kind = "binary", defines = "NEKO_PROTO_STATIC", packages = {"rapidjson", "spdlog", "gtest"}, files = {"src/proto_base.cpp", "tests/test_private_invoid_params.cpp"}, languages = "c++14", run_timeout = 1000})
    add_tests("private_cpp17", {group = "p", kind = "binary", defines = "NEKO_PROTO_STATIC", packages = {"rapidjson", "spdlog", "gtest"}, files = {"src/proto_base.cpp", "tests/test_private_invoid_params.cpp"}, languages = "c++17", run_timeout = 1000})
    add_tests("private_cpp20", {group = "p", kind = "binary", defines = "NEKO_PROTO_STATIC", packages = {"rapidjson", "spdlog", "gtest"}, files = {"src/proto_base.cpp", "tests/test_private_invoid_params.cpp"}, languages = "c++20", run_timeout = 1000})

    -- random proto test
    add_tests("rproto_cpp17", {group = "proto", kind = "binary", defines = "NEKO_PROTO_STATIC", packages = {"rapidjson", "spdlog", "gtest"}, files = {"tests/test_random_proto.cpp", "src/proto_base.cpp"}, languages = "c++17", run_timeout = 1000})
    add_tests("rproto_cpp20", {group = "proto", kind = "binary", defines = "NEKO_PROTO_STATIC", packages = {"rapidjson", "spdlog", "gtest"}, files = {"tests/test_random_proto.cpp", "src/proto_base.cpp"}, languages = "c++20", run_timeout = 1000})
    
    add_tests("bproto_cpp17", {group = "proto", kind = "binary", defines = "NEKO_PROTO_STATIC", packages = {"rapidjson", "spdlog", "gtest"}, files = {"tests/test_random_big_data.cpp", "src/proto_base.cpp"}, languages = "c++17", run_timeout = 3000})
    add_tests("bproto_cpp14", {group = "proto", kind = "binary", defines = "NEKO_PROTO_STATIC", packages = {"rapidjson", "spdlog", "gtest"}, files = {"tests/test_random_big_data.cpp", "src/proto_base.cpp"}, languages = "c++14", run_timeout = 3000})
    add_tests("bproto_cpp20", {group = "proto", kind = "binary", defines = "NEKO_PROTO_STATIC", packages = {"rapidjson", "spdlog", "gtest"}, files = {"tests/test_random_big_data.cpp", "src/proto_base.cpp"}, languages = "c++20", run_timeout = 3000})
target_end()


target("proto_cpp17")
    set_kind("binary")
    set_languages("c++17")
    add_defines("NEKO_PROTO_STATIC")
    add_packages("rapidjson", "spdlog", "gtest")
    add_tests("proto_cpp17")
    set_group("proto")
    add_files("src/proto_base.cpp")
    add_files("tests/test_proto.cpp")
target_end()

target("test_traits")
    set_kind("binary")
    set_languages("c++17")
    add_defines("NEKO_PROTO_STATIC")
    add_packages("rapidjson", "gtest", "spdlog")
    add_tests("traits_cpp17")
    set_group("traits")
    add_files("src/proto_base.cpp")
    add_files("tests/test_traits.cpp")
target_end()

target("test_communication")
    set_kind("binary")
    set_languages("c++20")
    add_includedirs("./modules/Ilias/include")
    add_defines("NEKO_PROTO_STATIC")
    add_defines("ILIAS_COROUTINE_LIFETIME_CHECK")
    add_packages("rapidjson", "gtest", "spdlog")
    add_tests("communication_cpp20")
    set_group("communication")
    add_files("src/communication_base.cpp")
    add_files("src/proto_base.cpp")
    add_files("tests/test_communication.cpp")
target_end()

option("ui_test")
    set_default(false)
    set_showmenu(true)
option_end()

option("cereal_test")
    set_default(false)
    set_showmenu(true)
option_end()

if has_config("cereal_test") then
add_requires("cereal")
target("test_cbproto")
    set_kind("binary")
    set_languages("c++17")
    add_packages("spdlog", "gtest", "cereal", "rapidjson")
    add_files("tests/test_random_big_data_cereal.cpp")
target_end()
end 

if has_config("ui_test") then 
    add_requires("qt6base")
    
    target("test_communication_ui")
        add_rules("qt.widgetapp")
        set_languages("c++20")
        add_includedirs("./modules/Ilias/include")
        add_files("tests/test_communication_ui.cpp")    
        add_files("tests/test_communication_ui.hpp")    
        add_files("tests/test_communication_ui.ui")    
        add_files("src/proto_base.cpp")
        add_files("src/communication_base.cpp")
        add_defines("NEKO_PROTO_STATIC")
        add_defines("ILIAS_COROUTINE_LIFETIME_CHECK")
        add_packages("rapidjson", "spdlog")
        add_frameworks("QtCore", "QtNetwork", "QtWidgets", "QtGui")
    target_end()
end 

if is_mode("coverage") and is_plat("linux") then 
    target("coverage-report")
        set_kind("phony")
        -- 执行脚本命令
        on_run(function (target)
            print("Generating coverage report...")
            -- 在这里编写您想要执行的脚本命令
            os.execv("lcov ", {"--capture", 
                "--directory", os.projectdir() .. "/build/.objs/", 
                "--output-file", os.projectdir() .. "/build/coverage.info",
                "--exclude", "/usr/include/*",
                "--exclude", "/usr/local/include/*",
                "--exclude", "*.xmake/packages/*",
                "--exclude", "*modules/Ilias/*",
                "--exclude", "*/tests/test_*",
                "--directory", os.projectdir()})
            os.execv("genhtml ", {
                "--output-directory", os.projectdir() .. "/build/coverage-report",
                "--title", "'Coverage Report'", 
                os.projectdir() .. "/build/coverage.info"})
        end)
    target_end()
end 
