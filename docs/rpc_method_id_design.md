# Neko-RPC method id 后端扩展设计

状态：初版已继续推进。当前实现提供 `NekoRpcBackend::Options`、`MethodIdMode`、hello 协商、连接期 full-table 同步、`table_version`/`signature_hash` TLV、endpoint 内部的 method name/method id 双向转换，以及非模板 base 层的 method-id 表状态机、delta TLV、兼容窗口、细分 method-id 错误和 compression TLV 协商。默认后端仍保持简易策略：hello 阶段发送 full table；连接内 method 表变化发送 delta hello；method-id 扩展错误会触发表刷新和一次字符串 method 回退重试；payload 压缩使用可替换 codec policy，默认内建 RunLength 算法，并在压缩后更小时才置 `Compressed` flag；压缩路径提供基础统计计数。

本文只讨论 `NekoRpcBackend` 的 wire-level 可选扩展。`method_id` 不属于 RPC frontend 的主要接口，也不应暴露到 `RpcMethod`、`RpcServer`、`RpcClient` 的调用语义中。调用方仍然按 method name 声明、绑定和调用；是否把 method name 压缩成 method id，是所选 backend 在封包/解包时做的优化。

RPC frontend 和 dispatcher 仍只感知字符串 method name；`flags.method_id` 只在 `NekoRpcBackend` 的 endpoint/session 内部被转换。

## 1. 定位

`method_id` 的目的很简单：在 Neko-RPC 二进制帧里用较短的数字 id 代替重复出现的字符串 method name，减少传输和查找成本。

它不是：

*   业务 API 的一部分。
*   `RpcMethod` 元数据的一部分。
*   JSON-RPC backend 的要求。
*   内建 introspection 方法的替代品。

它是：

*   `NekoRpcBackend` 的可选协商能力。
*   一种连接期优化。
*   可以通过后端专属 options 控制的行为。
*   可以协商失败并回退或拒绝连接的 backend 问题。

## 2. 对用户可见的形态

用户不应该在普通 RPC 调用处感知 method id：

```cpp
co_await client->calc.add(1, 2);
co_await client.callRemote<int>("calc.add", 1, 2);
```

上面的调用是否发字符串 method name、method id、压缩 payload，全部由 backend 决定。

如果用户确实想控制该能力，只通过后端专属配置表达，例如：

```cpp
using Backend = NekoRpcBackend<BinarySerializer>;

Backend::Options opts;
opts.methodId = Backend::MethodIdMode::Auto;    // Disable / Auto / Require
opts.compression = Backend::CompressionMode::Auto;
```

这些 options 属于 backend，不进入通用 RPC frontend 的概念和用户调用接口。

## 3. 协商策略

`hello` 只负责协商双方是否支持扩展，以及扩展是否启用。

建议 method id 模式：

```text
Disable: 不发送、不接受 method_id；收到 method_id 直接拒绝。
Auto:    双方都支持则启用；不支持则回退字符串 method。
Require: 必须启用；对端不支持或拒绝时协商失败。
```

协商失败是 backend/连接建立阶段的错误，不应改变 RPC frontend 的调用模型。

用户也可以完全绕过默认协商，自己在建立连接时处理 hello/TLV，然后把已经协商好的 endpoint/session 交给 backend 使用。默认 `NekoRpcBackend` 只提供一套简易、正确的内建协商方案。

## 4. 表生命周期

method id 表是连接期状态，不要求跨断线、跨服务重启保留历史。

规则：

*   连接建立后，如果启用 method id，服务端发送当前 method id 表或 delta。
*   客户端只在当前连接内使用这张表。
*   连接断开后，客户端必须重新协商或重新获取表，不能假设旧表仍然有效。
*   服务重启后不需要识别旧连接的历史 version；旧连接已经失效。
*   如果用户自己实现长缓存，那也是 backend opts/自定义协商的高级行为，不是默认保证。

因此默认方案不需要 `service epoch`。表版本只需要在同一条连接或同一次协商生命周期内判定新旧。

## 5. ID 分配

服务端拥有 method id 分配权。客户端不能自己根据 method list 下标发明 id。

默认分配规则：

*   服务端维护 `next_method_id`。
*   新增 method 时分配当前 `next_method_id`，然后递增。
*   当前连接生命周期内 id 不复用。
*   method 删除或签名不兼容变更时，旧 id 进入 removed/tombstone 状态，避免旧表误命中新 method。
*   如果服务端能兼容旧签名，可以保留旧 id 在兼容窗口内继续可用。

这些状态可以只保存在服务端内存中。默认后端不要求持久化。

## 6. 表版本

method id 表需要版本，但版本是连接/协商期版本，不是服务全局永久历史。

表版本建议为 `u64` 单调递增。以下情况递增：

*   新增 method。
*   删除 method。
*   method 签名或 serializer 兼容性变化。
*   tombstone/兼容状态变化。

客户端使用 method id 发请求时，必须携带自己所用的 `table_version`。服务端收到旧版本请求时：

*   如果旧版本仍在兼容窗口内，并且 id 能安全解析，可以继续执行。
*   response 可携带 backend warning/update TLV，提示客户端刷新表。
*   如果旧版本不可接受，返回明确的 backend 扩展错误，提示客户端刷新表后重试或回退字符串 method。

不需要为了断线重连维护无限历史。断线后重新协商即可。

## 7. Wire 语义建议

`flags.method_id` 表示 method 字段承载二进制 method id，而不是 UTF-8 method name。为了保持 fixed header 的 `method_size` 语义稳定：

```text
flags.method_id = 0:
  method bytes = UTF-8 method name

flags.method_id = 1:
  method_size = 8
  method bytes = u64be method_id
  extension TLV 携带 table_version
```

`table_version` TLV 可以是 critical：如果服务端不认识或不能接受，必须拒绝该 method-id 请求。字符串 method 请求不需要携带版本，始终是 fallback。

## 8. 表同步

默认 `NekoRpcBackend` 可以在 hello 阶段同步必要数据：

客户端 hello：

```text
supports_method_id
method_id_mode = auto | require
known_table_version optional
supports_table_delta optional
supports_compression optional
```

服务端 hello：

```text
method_id_enabled
current_table_version
full_table 或 delta
compression choice
```

表项建议包含：

```text
id: u64
name: string
signature_hash: optional
state: active | removed
```

当前默认 hello 仍发送 full table。连接内 method 表变化时，服务端 endpoint 会用同一套表状态机生成 delta hello；客户端 endpoint 在 `recv` 阶段吸收 hello 并继续等待真实 response。若服务端收到过期或失配的 method-id 请求，会先发送当前表更新，再返回细分 method-id 错误；客户端 endpoint 对这类错误自动用原始字符串 method 请求重试一次。

## 9. 请求处理

字符串 method 请求：

*   直接按 method name 调度。
*   不依赖 method id 表。
*   可作为任何 method-id 错误后的回退路径。

method id 请求：

1. 后端读取 `table_version`。
2. 后端根据当前连接保存的 method id 表解析 id 到 method name。
3. 如果版本太旧、id 不存在、id 已删除或签名不兼容，返回 backend 扩展错误。
4. 如果可兼容，后端把 method name 交给现有 dispatcher，RPC frontend 不需要知道请求曾经使用 method id。

## 10. 错误模型

method id 错误属于 Neko-RPC backend 扩展错误。建议至少区分：

```text
MethodIdNotNegotiated
MethodTableOutdated
MethodIdNotFound
MethodIdRemoved
MethodSignatureMismatch
MethodIdRequiredButUnsupported
CompressionRequiredButUnsupported
```

错误 payload 建议包含：

```text
current_table_version
optional full_table_or_delta_hint
```

客户端收到这些错误后，由 backend 决定刷新表、回退字符串 method、重试，或把协商失败报告给用户。普通 RPC 调用接口不需要新增参数。

## 11. 压缩与 method id

压缩是独立扩展，不和 method id 绑定。

建议处理顺序：

1. 解析固定 header。
2. 读取 extension TLV。
3. 如果 payload 压缩，按 compression TLV 解压。
4. 如果 `flags.method_id`，用连接内 method id 表还原 method name。
5. 用 serializer 解 payload。

extension TLV 自身保持未压缩，方便路由和错误恢复。

当前压缩扩展提供 `mode`、`algorithm` 和最小 payload size 策略 TLV。默认内建算法为 `RunLength`；双方都启用且 codec 支持该算法时协商成功，endpoint 在发送普通 request/response 前检查阈值并尝试压缩 payload。只有压缩结果更小时才设置 `Compressed` flag；接收端在 method-id 还原和 serializer 解码前解压 payload。`extension TLV` 自身始终保持未压缩。

压缩算法通过 `NekoRpcBackend<Serializer, CodecId, CompressionCodec>` 的第三个模板参数替换。codec policy 只需要提供 `preferredAlgorithm()`、`supports()`、`compress()` 和 `decompress()`，因此扩展新算法不需要修改 RPC frontend，也不需要把压缩细节暴露给普通调用方。`Options::compressionStats` 可以挂一个共享统计对象，用于观察压缩尝试次数、成功帧、跳过原因、输入/输出字节数和错误计数。

## 12. 对现有架构的约束

实现时应遵守：

*   不给 `RpcMethod` 增加 method id 字段。
*   不给用户调用 API 增加 method id 参数。
*   不要求所有 backend 实现 method id。
*   不把 method id 放进通用 `RpcBackend` concept。
*   不把 method id 设计成 RPC frontend 的主要接口。

允许的实现位置：

*   `NekoRpcBackend::Options` 控制默认协商策略。
*   `NekoRpcBackend` 的 endpoint/session 内部保存协商状态。
*   `NekoRpcBackend` 在 encode/decode 时把 method name 与 method id 互相转换。
*   Neko-RPC frame、TLV、hello 和 method-id table 的 wire 编解码放在 `backend_base.hpp` / `src/rpc.cpp` 的共享 helper 中。
*   默认 backend 可以利用已有的 method metadata 构建表，但这是库内部 wiring，不是用户可见接口。

## 13. 实现路线

1. [x] 保持字符串 method 调用为基础路径。
2. [x] 增加 `NekoRpcBackend::Options`，支持 method id `Disable/Auto/Require`。
3. [x] 增加 hello TLV 协商 method id。
4. [x] 第一版 method id 表只支持连接期 full table。
5. [x] 在 `NekoRpcBackend` endpoint 内部完成 method name 与 method id 转换。
6. [x] 增加 `table_version` TLV，并在请求侧校验当前连接表版本。
7. [x] 把 frame/TLV/hello/method table 编解码下沉到非模板 base 层。
8. [x] 增加动态更新测试：新增、删除、旧版本可兼容、旧版本不可兼容。
9. [x] 增加 delta、签名 hash、兼容窗口和更细 method-id 错误模型。
10. [x] 增加压缩 TLV 协商和压缩策略骨架。
11. [x] 增加连接内 method-id 表主动刷新/错误后自动重试策略。
12. [x] 增加真实 payload 压缩算法、压缩 flag 和阈值执行策略。
13. [x] 增加可替换 compression codec policy 和基础压缩统计。
14. [ ] 增加更强压缩算法和更完整调试观测。

## 14. 与内建 introspection 的区别

内建 introspection 是普通 RPC 方法，适合调试、发现和展示：

```text
rpc.get_method_list
rpc.get_bind_method_list
rpc.get_method_info
rpc.get_method_info_list
```

method id 表是 backend/wire 层优化状态。二者可以互相辅助，但不能互相替代。`rpc.get_method_list` 返回的是 method name 列表，不代表某个 method id 在某个连接表版本中有效。
