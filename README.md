# NekoProtoTools

[English](README_en.md) · [MIT License](LICENSE) · [Linux CI](https://github.com/liuli-neko/neko-proto-tools/actions/workflows/xmake-test-on-linux.yml) · [Windows CI](https://github.com/liuli-neko/neko-proto-tools/actions/workflows/xmake-test-on-windows.yml)

**NekoProtoTools 是一个 C++23 元数据工具库：同一份类型描述可用于反射、序列化、命令行解析和协议工具。** 当前开发版本见 [version.lua](version.lua)；稳定性状态见下文。

## 快速开始

下面示例使用内置 Binary 序列化器，无需第三方库。完整代码见 [serialization.cpp](examples/serialization.cpp)。

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
cmake -S . -B build -DNEKO_PROTO_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

## 核心能力

- **反射与 metadata**：非侵入 `Meta<T>`、字段遍历和 tags；见 [反射示例](examples/reflection.cpp)。
- **序列化**：内置 Binary；JSON、XML、YAML、TOML 后端按需启用，其中 CMake 当前已支持 RapidJSON；见 [序列化示例](examples/serialization.cpp)。
- **ArgParser**：从同一份 metadata 解析 CLI；见 [CLI 示例](examples/argparser.cpp)。
- **协议与 RPC**：xmake 已提供 Protocol、Communication 和 JSON/Binary RPC 目标；CMake 当前导出 Protocol，RPC 的 CMake 依赖映射仍在整理。

[一体化示例](examples/all_in_one.cpp) 使用同一份 `Meta<User>` 完成 CLI 解析、二进制往返和字段遍历。

## 集成

CMake 的 `FetchContent_MakeAvailable` 可直接使用：

```cmake
include(FetchContent)
FetchContent_Declare(NekoProtoTools GIT_REPOSITORY https://github.com/liuli-neko/neko-proto-tools.git GIT_TAG <固定提交或发布标签>)
FetchContent_MakeAvailable(NekoProtoTools)
target_link_libraries(your_app PRIVATE NekoProto::Serializer NekoProto::ArgParser)
```

安装后可使用 `find_package(NekoProtoTools CONFIG REQUIRED)` 和相同 targets。CMake 选项、RapidJSON 依赖以及 xmake 配置见 [构建说明](docs/build-options.md)。

xmake 用户可使用 `xmake f -m release --enable_rapidjson=y && xmake`；库的 xmake target 为 `NekoSerializer`、`NekoArgParser`、`NekoProtoBase`，可选 RPC target 为 `NekoJsonRpc`。

## 文档

[入门](docs/getting-started.md) · [反射](docs/reflection.md) · [序列化](docs/serialization.md) · [ArgParser](docs/argparser.md) · [协议与 RPC](docs/rpc.md) · [CMake 集成](docs/cmake-integration.md) · [构建选项](docs/build-options.md) · [Benchmark](benchmarks/README.md)

当前稳定性说明：反射、Binary 和 ArgParser 是正式 API 的候选；RPC 与跨版本 wire 兼容性仍在评估。欢迎通过 issue 或 PR 反馈使用问题。
