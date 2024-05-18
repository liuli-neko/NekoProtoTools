set_project("cc-proto")
add_rules("mode.debug", "mode.release", "mode.valgrind")
set_version("1.0.0")

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
    add_files("tests/test_co.cpp")
target_end()

target("proto_base")
    set_kind("static")
    add_defines("CS_PROTO_STATIC")
    set_languages("c++11")
    add_packages("spdlog")
    add_files("src/cc_proto_base.cpp")
target_end()

target("rpc_base")
    set_kind("static")
    add_defines("CS_RPC_STATIC")
    add_defines("CS_PROTO_STATIC")
    add_defines("ILIAS_COROUTINE_LIFETIME_CHECK")
    add_includedirs("./modules/Ilias/include")
    add_deps("proto_base")
    set_languages("c++20")
    add_packages("spdlog")
    add_files("src/cc_rpc_base.cpp")
target_end()

add_requires("gtest")
target("test_proto")
    set_kind("binary")
    add_defines("CS_PROTO_STATIC")
    set_languages("c++11")
    add_packages("rapidjson", "gtest", "spdlog")
    add_tests("proto")
    add_deps("proto_base")
    add_files("tests/test_proto.cpp")
target_end()

target("test_proto1")
    set_kind("binary")
    add_defines("CS_PROTO_STATIC")
    set_languages("c++17")
    add_packages("rapidjson", "gtest", "spdlog")
    add_deps("proto_base")
    add_tests("proto")
    set_group("serializer")
    add_files("tests/test_proto1.cpp")
target_end()

target("test_rpc")
    set_kind("binary")
    set_languages("c++20")
    add_includedirs("./modules/Ilias/include")
    add_defines("CS_PROTO_STATIC", "CS_RPC_STATIC")
    add_defines("ILIAS_COROUTINE_LIFETIME_CHECK")
    add_cxxflags("-Wstrict-aliasing")
    add_packages("rapidjson", "gtest", "spdlog")
    add_deps("proto_base", "rpc_base")
    add_tests("proto")
    set_group("rpc")
    add_files("tests/test_rpc.cpp")
target_end()

target("test_rpc_ui")
    add_rules("qt.widgetapp")
    set_languages("c++20")
    add_includedirs("./modules/Ilias/include")
    add_files("tests/test_rpc_ui.cpp")    
    add_files("src/cc_proto_base.cpp")
    add_files("src/cc_rpc_base.cpp")
    add_defines("CS_PROTO_STATIC", "CS_RPC_STATIC")
    add_defines("ILIAS_COROUTINE_LIFETIME_CHECK")
    add_packages("rapidjson", "spdlog")
target_end()

--
-- If you want to known more usage about xmake, please see https://xmake.io
--
-- ## FAQ
--
-- You can enter the project directory firstly before building project.
--
--   $ cd projectdir
--
-- 1. How to build project?
--
--   $ xmake
--
-- 2. How to configure project?
--
--   $ xmake f -p [macosx|linux|iphoneos ..] -a [x86_64|i386|arm64 ..] -m [debug|release]
--
-- 3. Where is the build output directory?
--
--   The default output directory is `./build` and you can configure the output directory.
--
--   $ xmake f -o outputdir
--   $ xmake
--
-- 4. How to run and debug target after building project?
--
--   $ xmake run [targetname]
--   $ xmake run -d [targetname]
--
-- 5. How to install target to the system directory or other output directory?
--
--   $ xmake install
--   $ xmake install -o installdir
--
-- 6. Add some frequently-used compilation flags in xmake.lua
--
-- @code
--    -- add debug and release modes
--    add_rules("mode.debug", "mode.release")
--
--    -- add macro definition
--    add_defines("NDEBUG", "_GNU_SOURCE=1")
--
--    -- set warning all as error
--    set_warnings("all", "error")
--
--    -- set language: c99, c++11
--    set_languages("c99", "c++11")
--
--    -- set optimization: none, faster, fastest, smallest
--    set_optimize("fastest")
--
--    -- add include search directories
--    add_includedirs("/usr/include", "/usr/local/include")
--
--    -- add link libraries and search directories
--    add_links("tbox")
--    add_linkdirs("/usr/local/lib", "/usr/lib")
--
--    -- add system link libraries
--    add_syslinks("z", "pthread")
--
--    -- add compilation and link flags
--    add_cxflags("-stdnolib", "-fno-strict-aliasing")
--    add_ldflags("-L/usr/local/lib", "-lpthread", {force = true})
--
-- @endcode
--

