if has_config("enable_tests") then
add_requires("gtest")

option("fuzzer_test")
    set_default(false)
    set_showmenu(true)
    set_description("Enable fuzzer test, should install libfuzzer")
    set_category("enable test")
option_end()

option("ui_test")
    set_default(false)
    set_showmenu(true)
    set_description("Enable ui test, should install qt")
    set_category("enable test")
option_end()

option("cereal_test")
    set_default(false)
    set_showmenu(true)
    set_category("enable test")
    set_description("Enable cereal test, should install cereal, to contrast with this")
option_end()

if is_plat("linux") then 
    option("memcheck")
        set_default(false)
        set_showmenu(true)
    option_end()
end 

add_defines("ILIAS_ENABLE_LOG")
-- Make all files in the directory into targets
for _, file in ipairs(os.files("./**/test_*.cpp")) do
    local name = path.basename(file)
    local dir = path.directory(file)
    local conf_path = dir .. "/" .. name .. ".lua"

    -- If this file require a specific configuration, load it, and skip the auto target creation
    if os.exists(conf_path) then 
        includes(conf_path)
        goto continue
    end

    -- Otherwise, create a target for this file, in most case, it should enough
    target(name)
        add_includedirs("$(projectdir)/include")
        set_kind("binary")
        set_default(false)
        set_encodings("utf-8")
        add_defines("SIMDJSON_EXCEPTIONS=1")
        add_defines("NEKO_PROTO_STATIC")
        add_packages("gtest")
        set_languages("c++17")
        add_files(file, "../src/proto_base.cpp")
        -- set test in different cpp versions
        local cpp_versions = {"c++17", "c++20"}
        for i = 1, #cpp_versions do
            add_tests(string.gsub(cpp_versions[i], '+', 'p', 2), {group = "proto", kind = "binary", files = {"../src/proto_base.cpp", file}, languages = cpp_versions[i], run_timeout = 30000})
        end
        if has_config("memcheck") then
            on_run(function (target)
                local argv = {}
                table.insert(argv, "--leak-check=full")
                table.insert(argv, target:targetfile())
                os.execv("valgrind", argv)
            end)
        end 
    target_end()

    ::continue::
end
end
