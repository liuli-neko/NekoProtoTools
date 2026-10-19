# NekoProtoTools

[English](README_en.md) · [MIT License](LICENSE) · [Linux CI](https://github.com/liuli-neko/neko-proto-tools/actions/workflows/xmake-test-on-linux.yml) · [Windows CI](https://github.com/liuli-neko/neko-proto-tools/actions/workflows/xmake-test-on-windows.yml)

**NekoProtoTools 是一个 C++20 元数据工具库：同一份类型描述可用于反射、序列化、命令行解析和协议工具。** 当前开发版本见 [version.lua](version.lua)；稳定性状态见下文。

## 快速开始

下面示例使用内置 Binary 序列化器，无需启用第三方格式后端。C++20 构建需要 zeus_expected；CMake 可查找已安装的头文件，也可按下面的配置获取缺失依赖。完整代码见 [serialization.cpp](examples/serialization.cpp)。

```cpp
#include <nekoproto/reflect.hpp>
#include <nekoproto/serialization/binary_serializer.hpp>
#include <string>
#include <vector>

struct User {
    int id = 7;
    std::string name = "Neko";
};

template <>
struct nekoproto::Meta<User> {
    static constexpr auto value = nekoproto::Object("id", &User::id, "name", &User::name);
};

int main() {
    std::vector<char> bytes;
    User source, copy;
    nekoproto::BinarySerializer::OutputSerializer out(bytes);
    if (!out(source)) return 1;
    nekoproto::BinarySerializer::InputSerializer in(bytes.data(), bytes.size());
    return in(copy) && copy.id == source.id && copy.name == source.name ? 0 : 2;
}
```

```sh
cmake -S . -B build -DNEKO_PROTO_BUILD_TESTS=ON -DNEKO_PROTO_FETCH_MISSING_DEPS=ON -DCMAKE_CXX_STANDARD=20
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

## 核心能力

- **反射与 metadata**：非侵入 `Meta<T>`、字段遍历和 tags；见 [反射示例](examples/reflection.cpp)。
- **序列化**：内置 Binary；JSON、XML、YAML、TOML 后端可按需用 CMake 或 xmake 启用；见 [序列化示例](examples/serialization.cpp)。
- **ArgParser**：从同一份 metadata 解析 CLI；见 [CLI 示例](examples/argparser.cpp)。
- **协议与 RPC**：两套构建系统均支持 Protocol、Communication 和 JSON/Binary RPC；[类型 ID 示例](examples/protocol_ids.cpp)展示进程间如何使用固定 ID。RPC 需要 Ilias，具体选项见 [构建说明](docs/build-options.md)。

[一体化示例](examples/all_in_one.cpp) 使用同一份 `Meta<User>` 完成 CLI 解析、二进制往返和字段遍历。

## 集成

CMake 的 `FetchContent_MakeAvailable` 可直接使用：

```cmake
include(FetchContent)
FetchContent_Declare(NekoProtoTools GIT_REPOSITORY https://github.com/liuli-neko/neko-proto-tools.git GIT_TAG <固定提交或发布标签>)
FetchContent_MakeAvailable(NekoProtoTools)
target_link_libraries(your_app PRIVATE NekoProto::Serializer NekoProto::ArgParser)
```

安装后可使用 `find_package(NekoProtoTools CONFIG REQUIRED)` 和相同 targets。CMake 与 xmake 的可选模块、依赖发现及缺失报告见 [构建说明](docs/build-options.md)。

xmake 用户可使用 `xmake f -m release --stdcxx=20 --enable_rapidjson=y && xmake`；库的 xmake target 为 `NekoSerializer`、`NekoArgParser`、`NekoProtoBase`，可选 RPC target 为 `NekoJsonRpc`。

## 文档

[入门](docs/getting-started.md) · [反射](docs/reflection.md) · [C++26 与模块](docs/cpp26.md) · [序列化](docs/serialization.md) · [ArgParser](docs/argparser.md) · [协议与 RPC](docs/rpc.md) · [CMake 集成](docs/cmake-integration.md) · [构建选项](docs/build-options.md) · [Benchmark](benchmarks/README.md)

## 稳定性范围

当前是 0.4.1 开发版本，尚未发布正式的跨版本兼容承诺。反射的 `Meta<T>` / `Object` / `Reflect<T>`、Binary 和 ArgParser 是公开 API 的稳定候选；`NEKO_SERIALIZER` 保留给已有代码，新代码推荐 `Meta<T>`。Protocol 已有显式类型 ID 的跨进程验证，但自动 ID 取决于注册顺序。RPC、Communication 和第三方后端扩展仍是实验性接口。Binary 与 RPC 的 wire 格式均不承诺跨发布版本兼容；格式后端也不承诺输出文本完全相同。详情见 [序列化](docs/serialization.md)、[协议](docs/protocol.md) 和 [RPC](docs/rpc.md)。欢迎通过 issue 或 PR 反馈使用问题。
