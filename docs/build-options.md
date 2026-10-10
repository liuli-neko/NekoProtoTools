# 构建选项

| 能力 | CMake | xmake |
|---|---|---|
| 反射、Binary、ArgParser | 默认 | `NekoSerializer` / `NekoArgParser` |
| C++20 `expected` 后备实现 | 自动查找 zeus_expected CMake 包或头文件；可选 FetchContent 下载 | 无标准库 `std::expected` 时自动引入 zeus_expected |
| Protocol | `NEKO_PROTO_ENABLE_PROTOCOL=ON`，导出 `NekoProto::Protocol` | `enable_protocol=y`，`NekoProtoBase` |
| RapidJSON | `NEKO_PROTO_ENABLE_RAPIDJSON=ON`，自动查找 CMake 包或可选 FetchContent 下载 | `enable_rapidjson=y` |
| yyjson | `NEKO_PROTO_ENABLE_YYJSON=ON`，自动查找 CMake 包或可选 FetchContent 下载 | `enable_yyjson=y` |
| simdjson | `NEKO_PROTO_ENABLE_SIMDJSON=ON`，自动查找 CMake 包或可选 FetchContent 下载 | `enable_simdjson=y` |
| pugixml / XML | `NEKO_PROTO_ENABLE_PUGIXML=ON`，自动查找 pugixml 包或可选 FetchContent 下载 | `enable_pugixml=y` |
| yaml-cpp / YAML | `NEKO_PROTO_ENABLE_YAMLCPP=ON`，自动查找 yaml-cpp 包或可选 FetchContent 下载 | `enable_yamlcpp=y` |
| libfyaml / YAML | `NEKO_PROTO_ENABLE_LIBFYAML=ON`，自动查找 libfyaml 包；Unix 可选 FetchContent 下载 | `enable_libfyaml=y` |
| toml++ / TOML | `NEKO_PROTO_ENABLE_TOMLPLUSPLUS=ON`，自动查找 tomlplusplus 包或可选 FetchContent 下载 | `enable_tomlplusplus=y` |
| 示例与运行检查 | `NEKO_PROTO_BUILD_EXAMPLES=ON` / `NEKO_PROTO_BUILD_TESTS=ON` | `enable_examples=y`，`xmake run example_reflection`、`xmake test -g examples` |
| 项目测试 | CMake 的 `NEKO_PROTO_BUILD_TESTS` 运行示例 smoke tests | `enable_tests=y`，依赖按需解析 |
| 三方 JSON benchmark | 不提供 | `enable_benchmarks=y`，`xmake run bench_json` |
| Communication | `NEKO_PROTO_ENABLE_COMMUNICATION=ON`，导出 `NekoProto::Communication`；需要 Protocol 和 Ilias | `enable_communication=y`，`NekoCommunication` |
| JSON-RPC / Neko RPC | `NEKO_PROTO_ENABLE_JSONRPC=ON`，导出 `NekoProto::JsonRpc`；需要 Ilias 及 RapidJSON 或 simdjson | `enable_jsonrpc=y`，`NekoJsonRpc` |
| RPC tracing | `NEKO_PROTO_ENABLE_RPC_TRACE=ON`，需要 JSON-RPC | `enable_rpc_trace=y` |

版本号只在根目录的 [`version.lua`](../version.lua) 修改；xmake 调用其中的函数，CMake 从同一行读取语义版本。CMake 的最低标准为 C++20，使用 [zeus_expected](https://github.com/zeus-cpp/expected) 提供 `expected`。CMake 先尝试 `find_package(zeus_expected CONFIG)`，再查找已安装的 `zeus/expected.hpp`；缺失时会报告可用的配置办法。可通过 `CMAKE_PREFIX_PATH` 指向依赖的安装前缀，或用 `NEKO_PROTO_ZEUS_EXPECTED_INCLUDE_DIR` 指向头文件目录。设置 `NEKO_PROTO_FETCH_MISSING_DEPS=ON` 可让 CMake 用 FetchContent 下载 v1.4.0；此选项也适用于安装后的消费者。xmake 在标准库 `std::expected` 不可用时自动引入 zeus_expected。

xmake 的测试、示例、JSON benchmark 默认关闭；开启相应选项才声明对应依赖。benchmark 的完整命令、结果格式和样例见 [`benchmarks/README.md`](../benchmarks/README.md)。

CMake 基础构建：

```sh
cmake -S . -B build -DNEKO_PROTO_BUILD_TESTS=ON -DCMAKE_CXX_STANDARD=20 -DNEKO_PROTO_FETCH_MISSING_DEPS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

RapidJSON、yyjson、simdjson 后端分别可加 `-DNEKO_PROTO_ENABLE_RAPIDJSON=ON`、`-DNEKO_PROTO_ENABLE_YYJSON=ON`、`-DNEKO_PROTO_ENABLE_SIMDJSON=ON`。CMake 先查找各自的 CMake 包；自动下载时使用与 xmake 相同的 RapidJSON 2025-02-05 提交、yyjson 0.12.0 和 simdjson v4.6.11。XML 后端可加 `-DNEKO_PROTO_ENABLE_PUGIXML=ON`，YAML 后端可加 `-DNEKO_PROTO_ENABLE_YAMLCPP=ON` 或 `-DNEKO_PROTO_ENABLE_LIBFYAML=ON`，TOML 后端可加 `-DNEKO_PROTO_ENABLE_TOMLPLUSPLUS=ON`；自动下载时分别使用 pugixml v1.16、yaml-cpp 0.8.0、libfyaml v0.9.5 和 toml++ v3.4.0。libfyaml 源码获取仅支持 Unix，Windows 需提供已安装的 CMake 包。安装包消费者从 `CMAKE_PREFIX_PATH` 查找这些包，缺失时可设置 `NEKO_PROTO_FETCH_MISSING_DEPS=ON`。

Communication 和 JSON-RPC 需要 Ilias 0.5.0。CMake 会查找导出 `ilias::ilias` 的安装包或 `ilias.pc`；可通过 `CMAKE_PREFIX_PATH` 或 `PKG_CONFIG_PATH` 指向安装位置。Ilias 暂不自动下载，缺失时配置会给出直接的依赖诊断。C++20 构建使用 Ilias 的 zeus_expected 配置。JSON-RPC 需同时打开 RapidJSON 或 simdjson；tracing 需先打开 JSON-RPC。这些选项会随安装包导出，消费者通过 `find_package(NekoProtoTools CONFIG REQUIRED)` 获得相应 target。
