# RPC 默认后端重构契约

这份文档约束 `NekoRpcBackend` 和 `NekoRpcFrameCodec` 的默认实现。后续重构以这里为准，避免把简单协议写成层层转发的深调用。

职责边界以 [`rpc_backend_context_model.md`](rpc_backend_context_model.md) 为准：endpoint 只负责 complete-message transport；扩展协商、method-id、压缩、错误解释和策略状态只能由 backend context/session 执行。

## 主线流程

RPC 默认后端的数据流拆成两条线：

- Transport 线：stream/datagram/file/pipe 到 complete message，只属于 endpoint。
- Backend 线：读取 Header、解析 method/extensions/payload、执行 backend 扩展、生成 logical request/response，只属于 backend context/session。

扩展只能在 Backend 线增加一个明确步骤，不能进入 endpoint，也不能把顺序藏进多层 helper。helper 只保留在真实边界：

- Header 的反射读写。
- TLV 的装载、写回、按类型读取值。
- method table 的 value 编解码。
- payload 压缩算法本身。

不保留只改名、不增加语义的转发函数；同一个概念不要拆成一组一字段一函数。

## 结构边界

- `Header` 是固定头，字段必须自解释并带注释，读写由反射遍历完成。
- `FrameParts` 是一帧被拆开后的线性视图：`header`、`method`、`extensions`、`payload`。
- `ExtensionMap` 是已解析 TLV 的主结构，按 `NekoRpcExtensionType` 索引；业务代码不再手动拼接一段裸 TLV 字节流。
- 对外编码帧使用 `FrameParts`，不要恢复超长参数列表。
- `ClientContext`、`ServerContext`、`PeerSession` 是扩展状态的归属位置；RPC API 只负责保存和传递，不访问内部字段。
- Endpoint 不保存 method table、压缩协商结果或 hello 状态。

## 扩展目录

扩展必须集中列在 `NekoRpcExtensionType`，并说明出现位置和影响：

| 扩展 | 出现位置 | 影响 |
| --- | --- | --- |
| `MethodId` | Client Hello 表示支持；Server Hello 表示本连接启用 | 启用后请求/通知可设置 `NekoRpcFlag::MethodId`，`method` 段变成 8 字节 method id |
| `MethodTableVersion` | Server Hello；MethodId 请求；MethodId 错误 response | Server 发布当前表版本；请求回传版本用于检测旧表 |
| `MethodTable` | Server Hello；MethodId 错误 response；显式刷新 response | 在预算内下发完整 method-id 表 |
| `MethodTableDelta` | Server Hello；MethodId 错误 response；显式刷新 response | 在预算内下发 method-id 表增量 |
| `MethodSignatureHash` | MethodId 请求 | 校验客户端 method 元数据是否和服务端一致 |
| `MethodMinimumCompatibleVersion` | Server Hello/table refresh | 告诉客户端服务端还能接受的最低 method table 版本 |
| `Compression` | Client Hello 表示支持；Server Hello 表示本连接启用 | 启用后非 Hello 帧可设置 `NekoRpcFlag::Compressed`，payload 按协商算法压缩 |
| `CompressionAlgorithm` | Hello | 客户端给出支持算法，服务端回复选中的算法 |
| `CompressionMinPayloadSize` | Hello | 双方确认本连接最小压缩 payload 大小 |

## Hello 协商

Hello 只做能力声明和启用确认：

1. 客户端 Hello 发送自己支持的扩展 key，以及必要的简单参数。
2. 服务端取客户端支持和本地能力的交集，生成该客户端专属 peer session。
3. 服务端 Hello 只返回实际启用的扩展和 context 数据。
4. `Auto`/`Require` 不再作为远端协商模式传播；它们只表示本端除 `Disable` 外愿意启用该能力。
5. 服务端不能因为客户端没有启用某个扩展而拒绝整个 Hello；未启用就走基础字符串 method / 未压缩 payload 路径。
6. 服务端 Hello 和 method-id 错误 response 只有在 method table 数据落入 `max_auto_method_table_extension_bytes` 预算内时才自动附表；默认预算是一帧 extension 区。

## 实现约束

- 代码优先表达协议步骤，不追求类型数量。
- 使用结构承载一组相关字段，避免长参数列表。
- 扩展处理沿 `ExtensionMap` 线性读取和写回，避免先拼 TLV 再拆 TLV。
- 日志可以说明启用了什么，但不引入额外协商状态机。
- 不给 endpoint 增加 backend 专属方法；用户自定义 endpoint 不需要接入任何扩展能力。
- 不在服务端自动处理 method-id 失败后的刷新、重试或字符串 method 回退；服务端只返回明确 backend error，并可在预算内附带恢复用 TLV。
- 测试覆盖应跑 RPC 单测；Windows CI 已能直接开启全部自动测试，新增测试不需要绕开 RPC。
