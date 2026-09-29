# 序列化

`BinarySerializer` 内置且不依赖第三方库；`JsonSerializer` 需要启用 RapidJSON 或 simdjson。xmake 还支持 pugixml、libfyaml/yaml-cpp 与 toml++。CMake 当前验证了 Binary 与 RapidJSON 两条路径；其余 CMake 后端尚未导出依赖 target。

新代码推荐用 `Meta<T>` 描述字段；`NEKO_SERIALIZER` 宏仍保留给已有代码，现有单元测试继续覆盖它。用 `OutputSerializer` 写入 `std::vector<char>`，检查调用结果；用 `InputSerializer` 从字节范围读取并再次检查结果。完整代码在 [examples/serialization.cpp](../examples/serialization.cpp)。反序列化外部输入时必须处理失败结果，不要把 Binary V2 字节布局视为跨版本稳定协议。

可选后端的构建入口见 [构建选项](build-options.md)。

Binary 输入只接受一个根值。失败时调用返回 `false`，可通过 `error()` 读取 `sa::Error` 的错误码和消息；再次调用同一输入对象仍会失败。对普通的可复制且可移动赋值的目标，Binary 会先解码到副本，完整解析成功后才提交；不可复制目标或含引用的包装值不保证失败时保持原值。`IProto::fromData` 只返回成功/失败，不暴露底层错误对象。
