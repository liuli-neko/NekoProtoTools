neko_define_test(path.join(os.scriptdir(), "test_json_backend.cpp"),
                 has_config("enable_rapidjson") or has_config("enable_simdjson"))
