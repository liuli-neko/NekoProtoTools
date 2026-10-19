# RPC

公共入口是 `<nekoproto/rpc/rpc.hpp>`。`RpcClient<Backend, ProtocolSets...>` 与 `RpcServer<Backend, ProtocolSets...>` 提供协议注册、方法调用和 endpoint 接入；`BinaryRpcBackend` 是内置 Binary 后端，`NekoRpcBackend<Serializer, CodecId, CompressionCodec>` 提供可配置的后端实现。异步任务与传输依赖 Ilias。xmake 可构建 `NekoJsonRpc`；CMake 设置 `NEKO_PROTO_ENABLE_JSONRPC=ON` 后导出 `NekoProto::JsonRpc`，并可用 `NEKO_PROTO_ENABLE_RPC_TRACE=ON` 加入 tracing。

调用端可用 `RpcCallOptions` 指定相对超时、绝对 deadline 或取消令牌；服务端处理器可通过 `RpcRequestContext` 查看本次调用的 method、request ID、deadline、取消状态及由接入端提供的 `RpcPeerInfo`。客户端和服务端均有 endpoint、`close()`、`shutdown()` 及指标接口；调用失败通过 Ilias 任务结果和 `RpcError` 报告。详细用法目前以 [RPC 单元测试](../tests/unit/rpc/test_neko_rpc_backend.cpp) 为准。

Neko RPC 普通帧以 `4E 52 01`（`NR`、wire 版本 1）开头，固定头的多字节整数按大端序编码，之后依次是方法标识、TLV 扩展和序列化负载。接收端会拒绝未知 wire 版本；`codec` 字段区分负载序列化方式，方法表的版本号是独立的扩展字段。测试中已固定包含 TLV 与 Binary V2 负载的完整 V1 帧向量。若要改变 V1 帧头或已有字段的解释，应升级 wire 版本并设计迁移；当前仍不承诺跨发布版本的 RPC wire 兼容性。

RPC 仍是实验性模块。`RpcBackend` concept、可替换压缩 codec、方法 ID 协商和公共头内的 `private/`、`detail/` 实现尚不构成稳定的第三方后端扩展承诺。本地 C++20 `test_neko_rpc_backend` 的 41 项测试覆盖多连接、取消、超时、关闭、负载限制、方法表更新和固定帧向量；远端 Windows xmake MSVC、clang-cl 与 Linux C++20 常规测试已通过。CMake C++20 下 Linux/GCC 与 Windows/MSVC 的 RPC 构建、示例、安装包和源码消费者已验证 target、依赖链接和运行路径；Windows 使用 MSVC 构建的 Ilias 共享包。
