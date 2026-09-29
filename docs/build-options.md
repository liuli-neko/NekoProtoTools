# 构建选项

| 能力 | CMake | xmake |
|---|---|---|
| 反射、Binary、ArgParser | 默认 | `NekoSerializer` / `NekoArgParser` |
| Protocol | `NEKO_PROTO_ENABLE_PROTOCOL=ON`，导出 `NekoProto::Protocol` | `enable_protocol=y`，`NekoProtoBase` |
| RapidJSON | `NEKO_PROTO_ENABLE_RAPIDJSON=ON`，需可发现 RapidJSON CMake package | `enable_rapidjson=y` |
| 示例与运行检查 | `NEKO_PROTO_BUILD_EXAMPLES=ON` / `NEKO_PROTO_BUILD_TESTS=ON` | `enable_examples=y`，`xmake run example_reflection`、`xmake test -g examples` |
| 项目测试 | CMake 的 `NEKO_PROTO_BUILD_TESTS` 运行示例 smoke tests | `enable_tests=y`，依赖按需解析 |
| 三方 JSON benchmark | 不提供 | `enable_benchmarks=y`，`xmake run bench_json` |
| 其他格式后端、Communication、RPC | 尚未映射到 CMake | 见 [`xmake.lua`](../xmake.lua) |

版本号只在根目录的 [`version.lua`](../version.lua) 修改；xmake 调用其中的函数，CMake 从同一行读取语义版本。CMake 要求 C++23 标准库的 `std::expected`。xmake 在其不可用时会拉取 zeus_expected。

xmake 的测试、示例、JSON benchmark 默认关闭；开启相应选项才声明对应依赖。benchmark 的完整命令、结果格式和样例见 [`benchmarks/README.md`](../benchmarks/README.md)。

CMake 基础构建：

```sh
cmake -S . -B build -DNEKO_PROTO_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

CMake 当前聚焦核心库及下游安装集成。尚未覆盖所有 xmake 可选模块。
