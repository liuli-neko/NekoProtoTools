# 入门

要求：C++20 编译器与 CMake 3.23+。Binary、反射和 ArgParser 不需要格式后端。源码根目录执行：

```sh
cmake -S . -B build -DNEKO_PROTO_BUILD_TESTS=ON -DNEKO_PROTO_FETCH_MISSING_DEPS=ON -DCMAKE_CXX_STANDARD=20
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

`examples/common/user.hpp` 用非侵入的 `Meta<User>` 定义字段。这个描述同时被 [反射](reflection.md)、[序列化](serialization.md) 和 [ArgParser](argparser.md) 使用。需要 JSON 时可开启 `-DNEKO_PROTO_ENABLE_RAPIDJSON=ON`、`-DNEKO_PROTO_ENABLE_YYJSON=ON` 或 `-DNEKO_PROTO_ENABLE_SIMDJSON=ON`；用 `CMAKE_PREFIX_PATH` 提供已安装的依赖，或设置 `-DNEKO_PROTO_FETCH_MISSING_DEPS=ON` 允许 CMake 获取缺失依赖。

详细用法从对应模块页和 `examples/` 开始；构建选项见 [构建说明](build-options.md)。
