# Protocol 与 Communication

`ProtoFactory`/`IProto` 提供协议对象与工厂功能；CMake 导出 `NekoProto::Protocol`，xmake target 名为 `NekoProtoBase`。Communication 依赖 Ilias，目前仅由 xmake 构建。

协议工厂接口位于 `<nekoproto/proto/proto_base.hpp>`。构造时的版本号按每段低 8 位打包为 `major << 16 | minor << 8 | patch`；它只是工厂元数据，不参与 Binary 输入的版本校验。

自动类型 ID 从 65 起按注册顺序分配，不按名称排序。需要固定 ID 时，`specifyProtoType<T>(id)` 只接受 1–64；无效或冲突的 ID 返回 `-1`。运行时 `regist` 仅为当前工厂设置创建器，同名再次注册会替换创建器；空名称或空创建器会被忽略。注册名称必须具有足够长的生命周期，因为全局名称映射保存的是 `std::string_view`。未知名称、空名称、空指针名称和未注册类型的 `create` 返回空 `IProto`；调用者可与 `nullptr` 比较。工厂创建的 `IProto` 自行管理对象生命周期；实例的 `makeProto()` 仅借用该实例。

跨版本 wire 兼容性目前没有稳定承诺。
