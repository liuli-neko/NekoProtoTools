# RPC

公共入口是 `<nekoproto/rpc/rpc.hpp>`。`RpcClient<Backend, ProtocolSets...>` 与 `RpcServer<Backend, ProtocolSets...>` 提供协议注册、方法调用和 endpoint 接入；`BinaryRpcBackend` 是内置 Binary 后端，`NekoRpcBackend<Serializer, CodecId, CompressionCodec>` 提供可配置的后端实现。异步任务与传输依赖 Ilias。当前 xmake 可构建 `NekoJsonRpc`；CMake 尚未导出 RPC target。

调用端可用 `RpcCallOptions` 指定相对超时、绝对 deadline 或取消令牌；服务端处理器可通过 `RpcRequestContext` 查看本次调用的 method、request ID、deadline、取消状态及由接入端提供的 `RpcPeerInfo`。客户端和服务端均有 endpoint、`close()`、`shutdown()` 及指标接口；调用失败通过 Ilias 任务结果和 `RpcError` 报告。详细用法目前以 [RPC 单元测试](../tests/unit/rpc/test_neko_rpc_backend.cpp) 为准。

RPC 仍是实验性模块。`RpcBackend` concept、可替换压缩 codec、方法 ID 协商和公共头内的 `private/`、`detail/` 实现尚不构成稳定的第三方后端扩展承诺。本地 `test_neko_rpc_backend` 的 40 项测试覆盖多连接、取消、超时、关闭、负载限制和方法表更新；跨版本 wire golden vector、CMake 下游依赖导出和远端平台运行尚未验证。
