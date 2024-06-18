set_project("cc-proto")
add_rules("mode.debug", "mode.release", "mode.valgrind", "mode.coverage")
set_version("1.0.0")

add_rules("plugin.compile_commands.autoupdate", {outputdir = ".vscode"})

add_requires("rapidjson", "spdlog")

if is_mode("debug") then
    add_defines("CS_CCPROTO_LOG_CONTEXT")
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

target("rpc_base")
    set_kind("static")
    add_defines("CS_RPC_STATIC")
    add_defines("CS_PROTO_STATIC")
    add_defines("ILIAS_COROUTINE_LIFETIME_CHECK")
    add_includedirs("./modules/Ilias/include")
    set_languages("c++20")
    add_packages("spdlog")
    add_files("src/cc_proto_base.cpp")
    add_files("src/cc_rpc_base.cpp")
target_end()

add_requires("gtest")
target("test_js")
    set_kind("binary")
    add_defines("CS_PROTO_STATIC")
    set_languages("c++17")
    add_packages("rapidjson", "gtest", "spdlog")
    add_tests("proto")
    add_files("src/cc_proto_base.cpp")
    add_files("tests/test_json_serializer.cpp")
target_end()

target("test_proto")
    set_kind("binary")
    add_defines("CS_PROTO_STATIC")
    set_languages("c++20")
    add_packages("rapidjson", "gtest", "spdlog")
    add_tests("proto")
    set_group("serializer")
    add_files("src/cc_proto_base.cpp")
    add_files("tests/test_proto.cpp")
target_end()

target("test_rpc")
    set_kind("binary")
    set_languages("c++20")
    add_includedirs("./modules/Ilias/include")
    add_defines("CS_PROTO_STATIC", "CS_RPC_STATIC")
    add_defines("ILIAS_COROUTINE_LIFETIME_CHECK")
    add_cxxflags("-Wstrict-aliasing")
    add_packages("rapidjson", "gtest", "spdlog")
    add_deps("rpc_base")
    add_tests("proto")
    set_group("rpc")
    add_files("tests/test_rpc.cpp")
target_end()

target("test_dump")
    set_kind("binary")
    set_languages("c++20")
    add_includedirs("./modules/Ilias/include")
    add_defines("CS_PROTO_STATIC", "CS_RPC_STATIC")
    add_defines("ILIAS_COROUTINE_LIFETIME_CHECK")
    add_packages("rapidjson", "gtest", "spdlog")
    add_deps("rpc_base")
    add_tests("proto")
    set_group("so")
    add_files("tests/test_dump_object.cpp")
target_end()

target("test_bs")
    set_kind("binary")
    set_languages("c++20")
    add_includedirs("./modules/Ilias/include")
    add_defines("CS_PROTO_STATIC", "CS_RPC_STATIC")
    add_defines("ILIAS_COROUTINE_LIFETIME_CHECK")
    add_packages("rapidjson", "gtest", "spdlog")
    add_tests("proto")
    set_group("bs")
    add_files("tests/test_binary_serializer.cpp")
target_end()

option("ui_test")
    set_default(false)
    set_showmenu(true)
option_end()

if has_config("ui_test") then 
    add_requires("qt6base")
    
    target("test_rpc_ui")
        add_rules("qt.widgetapp")
        set_languages("c++20")
        add_includedirs("./modules/Ilias/include")
        add_files("tests/test_rpc_ui.cpp")    
        add_files("tests/test_rpc_ui.hpp")    
        add_files("tests/test_rpc_ui.ui")    
        add_files("src/cc_proto_base.cpp")
        add_files("src/cc_rpc_base.cpp")
        add_defines("CS_PROTO_STATIC", "CS_RPC_STATIC")
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