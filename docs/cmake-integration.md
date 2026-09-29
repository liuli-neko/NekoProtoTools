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

若启用 `NEKO_PROTO_ENABLE_RAPIDJSON`，配置本项目和下游时都要让 RapidJSON 的 CMake package 可被发现，例如传入相应的 `CMAKE_PREFIX_PATH`。可运行的下游验证工程在 [tests/cmake/consumer](../tests/cmake/consumer)。
