# CMake 集成

源码集成：

```cmake
include(FetchContent)
FetchContent_Declare(NekoProtoTools GIT_REPOSITORY https://github.com/liuli-neko/neko-proto-tools.git GIT_TAG <固定提交或标签>)
FetchContent_MakeAvailable(NekoProtoTools)
target_link_libraries(app PRIVATE NekoProto::Serializer NekoProto::ArgParser)
```

安装包集成：

```sh
cmake -S . -B build
cmake --build build
cmake --install build --prefix /your/prefix
```

```cmake
find_package(NekoProtoTools CONFIG REQUIRED)
target_link_libraries(app PRIVATE NekoProto::Serializer)
```

启用可选后端或 RPC 后，本项目和安装包消费者都需能找到相应依赖。用 `CMAKE_PREFIX_PATH` 指向已安装的依赖，或设置 `NEKO_PROTO_FETCH_MISSING_DEPS=ON` 允许 CMake 获取缺失依赖，包括 Ilias v0.5.1。自动获取 Ilias 需要 CMake 3.25 或更新版本；已有 Ilias 安装包仍可在本项目最低要求的 CMake 3.23 下使用。缺失时 CMake 会报告依赖名称及可用配置方式。各模块开关和限制见 [构建选项](build-options.md)；可运行的下游验证工程在 [tests/cmake/consumer](../tests/cmake/consumer)。
