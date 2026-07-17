neko_define_test(path.join(os.scriptdir(), "test_yaml.cpp"),
                 has_config("enable_libfyaml") or has_config("enable_yamlcpp"))
