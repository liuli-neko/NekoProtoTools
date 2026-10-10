# 序列化

`BinarySerializer` 内置且不依赖第三方库；`JsonSerializer` 需要启用 yyjson、RapidJSON 或 simdjson。CMake 已支持 `NEKO_PROTO_ENABLE_YYJSON=ON`、`NEKO_PROTO_ENABLE_RAPIDJSON=ON` 和 `NEKO_PROTO_ENABLE_SIMDJSON=ON`，并向下游传递依赖。XML 需要 pugixml，CMake 可用 `NEKO_PROTO_ENABLE_PUGIXML=ON` 启用。YAML 的 yaml-cpp 和 libfyaml 后端分别可用 `NEKO_PROTO_ENABLE_YAMLCPP=ON`、`NEKO_PROTO_ENABLE_LIBFYAML=ON` 启用；TOML 后端可用 `NEKO_PROTO_ENABLE_TOMLPLUSPLUS=ON` 启用。

新代码推荐用 `Meta<T>` 描述字段；`NEKO_SERIALIZER` 宏仍保留给已有代码，现有单元测试继续覆盖它。用 `OutputSerializer` 写入 `std::vector<char>`，检查调用结果；用 `InputSerializer` 从字节范围读取并再次检查结果。完整代码在 [examples/serialization.cpp](../examples/serialization.cpp)。反序列化外部输入时必须处理失败结果，不要把 Binary V2 字节布局视为跨版本稳定协议。

可选后端的构建入口见 [构建选项](build-options.md)。

Binary 输入只接受一个根值。普通帧以 `4E 50 02` 开头，最后一个字节是当前 V2 wire 版本；`NP` 后跟未知版本会被拒绝。`raw_fixed_data` 是显式选择的无帧记录，必须由双方预先约定完全相同的字段布局。V2 的标量、variant 和反射对象布局已有固定字节回归测试；将来若要改变现有标签的含义或容器语法，须使用新 wire 版本并单独设计迁移。当前仍不承诺跨发布版本兼容。失败时调用返回 `false`，可通过 `error()` 读取 `sa::Error` 的错误码和消息；再次调用同一输入对象仍会失败。对普通的可复制且可移动赋值的目标，Binary 会先解码到副本，完整解析成功后才提交；不可复制目标或含引用的包装值不保证失败时保持原值。`IProto::fromData` 只返回成功/失败，不暴露底层错误对象。
