# NekoProtoTools

[中文](README.md) · [MIT License](LICENSE) · [Linux CI](https://github.com/liuli-neko/neko-proto-tools/actions/workflows/xmake-test-on-linux.yml) · [Windows CI](https://github.com/liuli-neko/neko-proto-tools/actions/workflows/xmake-test-on-windows.yml)

**NekoProtoTools is a C++20 metadata toolkit: one type description can drive reflection, serialization, command-line parsing, and protocol tooling.** See [version.lua](version.lua) for the development version; stability status is summarized below.

## Quick start

This uses the built-in binary serializer and needs no optional format backend. C++20 builds require zeus_expected; CMake can find an installed copy or fetch the missing dependency with the configuration below. The complete program is in [serialization.cpp](examples/serialization.cpp).

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

## Capabilities

- **Reflection and metadata**: non-intrusive `Meta<T>`, field iteration, and tags; see the [reflection example](examples/reflection.cpp).
- **Serialization**: built-in Binary and optional JSON, XML, YAML, and TOML backends through CMake or xmake; see the [serialization example](examples/serialization.cpp).
- **ArgParser**: parse CLI options using the same metadata; see the [CLI example](examples/argparser.cpp).
- **Protocol and RPC**: both build systems support Protocol, Communication, and JSON/Binary RPC. The [type ID example](examples/protocol_ids.cpp) shows why protocols exchanged between processes need fixed IDs. RPC requires Ilias; see [build options](docs/build-options.md).

The [all-in-one example](examples/all_in_one.cpp) uses one `Meta<User>` for CLI parsing, binary round trips, and field iteration.

## Integration

Use `FetchContent_MakeAvailable` directly:

```cmake
include(FetchContent)
FetchContent_Declare(NekoProtoTools GIT_REPOSITORY https://github.com/liuli-neko/neko-proto-tools.git GIT_TAG <pinned-commit-or-release-tag>)
FetchContent_MakeAvailable(NekoProtoTools)
target_link_libraries(your_app PRIVATE NekoProto::Serializer NekoProto::ArgParser)
```

After installation, use `find_package(NekoProtoTools CONFIG REQUIRED)` with the same targets. See [build options](docs/build-options.md) for optional modules, dependency discovery, missing dependency diagnostics, and xmake configuration.

For xmake, use `xmake f -m release --stdcxx=20 --enable_rapidjson=y && xmake`. Its targets are `NekoSerializer`, `NekoArgParser`, `NekoProtoBase`, and optional `NekoJsonRpc`.

## Documentation

[Getting started](docs/getting-started.md) · [Reflection](docs/reflection.md) · [C++26 and modules](docs/cpp26.md) · [Serialization](docs/serialization.md) · [ArgParser](docs/argparser.md) · [Protocol and RPC](docs/rpc.md) · [CMake integration](docs/cmake-integration.md) · [Build options](docs/build-options.md) · [Benchmark](benchmarks/README.md)

## Stability scope

This is the 0.4.1 development version; no cross-version compatibility promise has been published yet. Reflection through `Meta<T>` / `Object` / `Reflect<T>`, Binary, and ArgParser are candidates for stable public APIs. `NEKO_SERIALIZER` remains for existing code; new code should prefer `Meta<T>`. Protocol has cross-process coverage for explicit type IDs, while automatic IDs depend on registration order. RPC, Communication, and third-party backend extensions remain experimental. Neither Binary nor RPC promises wire compatibility across releases, and format backends do not promise identical output text. See [serialization](docs/serialization.md), [protocol](docs/protocol.md), and [RPC](docs/rpc.md) for details. Issues and PRs are welcome.
