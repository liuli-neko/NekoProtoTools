# 入门

要求：C++23 编译器与 CMake 3.23+。Binary、反射和 ArgParser 不需要格式后端。源码根目录执行：

```sh
cmake -S . -B build -DNEKO_PROTO_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
./build/examples/neko_all_in_one --id 7 --name Neko
```

`examples/common/user.hpp` 用非侵入的 `Meta<User>` 定义字段。这个描述同时被 [反射](reflection.md)、[序列化](serialization.md) 和 [ArgParser](argparser.md) 使用。需要 JSON 时开启 `-DNEKO_PROTO_ENABLE_RAPIDJSON=ON`，并让 CMake 能找到 RapidJSON 的 package config。

详细用法从对应模块页和 `examples/` 开始；构建选项见 [构建说明](build-options.md)。
