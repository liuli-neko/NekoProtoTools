if has_config("cereal_test") then
    add_requires("cereal")
    target("test_cbproto")
        set_kind("binary")
        set_languages("c++17")
        add_packages("gtest", "cereal")
        add_options("enable_spdlog", "enable_fmt", "enable_stdformat", "enable_rapidjson", "enable_simdjson")
        add_includedirs("../../../")
        add_files("test_random_big_data_cereal.cpp")
    target_end()
end 