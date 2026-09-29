# NekoProtoTools

[中文](README.md) · [MIT License](LICENSE) · [Linux CI](https://github.com/liuli-neko/neko-proto-tools/actions/workflows/xmake-test-on-linux.yml) · [Windows CI](https://github.com/liuli-neko/neko-proto-tools/actions/workflows/xmake-test-on-windows.yml)

**NekoProtoTools is a C++23 metadata toolkit: one type description can drive reflection, serialization, command-line parsing, and protocol tooling.** See [version.lua](version.lua) for the development version; stability status is summarized below.

## Quick start

This uses the built-in binary serializer and needs no third-party library. The complete program is in [serialization.cpp](examples/serialization.cpp).

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

## Capabilities

- **Reflection and metadata**: non-intrusive `Meta<T>`, field iteration, and tags; see the [reflection example](examples/reflection.cpp).
- **Serialization**: built-in Binary and optional JSON, XML, YAML, and TOML backends. CMake currently supports RapidJSON; see the [serialization example](examples/serialization.cpp).
- **ArgParser**: parse CLI options using the same metadata; see the [CLI example](examples/argparser.cpp).
- **Protocol and RPC**: xmake supports Protocol, Communication, and JSON/Binary RPC. CMake currently exports Protocol; its RPC dependency mapping is still being prepared.

The [all-in-one example](examples/all_in_one.cpp) uses one `Meta<User>` for CLI parsing, binary round trips, and field iteration.

## Integration

Use `FetchContent_MakeAvailable` directly:

```cmake
include(FetchContent)
FetchContent_Declare(NekoProtoTools GIT_REPOSITORY https://github.com/liuli-neko/neko-proto-tools.git GIT_TAG <pinned-commit-or-release-tag>)
FetchContent_MakeAvailable(NekoProtoTools)
target_link_libraries(your_app PRIVATE NekoProto::Serializer NekoProto::ArgParser)
```

After installation, use `find_package(NekoProtoTools CONFIG REQUIRED)` with the same targets. See [build options](docs/build-options.md) for CMake, RapidJSON, and xmake configuration.

For xmake, use `xmake f -m release --enable_rapidjson=y && xmake`. Its targets are `NekoSerializer`, `NekoArgParser`, `NekoProtoBase`, and optional `NekoJsonRpc`.

## Documentation

[Getting started](docs/getting-started.md) · [Reflection](docs/reflection.md) · [Serialization](docs/serialization.md) · [ArgParser](docs/argparser.md) · [Protocol and RPC](docs/rpc.md) · [CMake integration](docs/cmake-integration.md) · [Build options](docs/build-options.md) · [Benchmark](benchmarks/README.md)

Reflection, Binary, and ArgParser are candidates for stable APIs. RPC and cross-version wire compatibility remain under review. Issues and PRs are welcome.
