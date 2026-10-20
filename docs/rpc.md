# RPC

公共入口是 `<nekoproto/rpc/rpc.hpp>`。`RpcClient<Backend, ProtocolSets...>` 与 `RpcServer<Backend, ProtocolSets...>` 提供协议注册、方法调用和 endpoint 接入；`BinaryRpcBackend` 是内置 Binary 后端，`NekoRpcBackend<Serializer, CodecId, CompressionCodec>` 提供可配置的后端实现。异步任务与传输依赖 Ilias。xmake 可构建 `NekoJsonRpc`；CMake 设置 `NEKO_PROTO_ENABLE_JSONRPC=ON` 后导出 `NekoProto::JsonRpc`，并可用 `NEKO_PROTO_ENABLE_RPC_TRACE=ON` 加入 tracing。

调用端可用 `RpcCallOptions` 指定相对超时、绝对 deadline 或取消令牌；服务端处理器可通过 `RpcRequestContext` 查看本次调用的 method、request ID、deadline、取消状态及由接入端提供的 `RpcPeerInfo`。客户端和服务端均有 endpoint、`close()`、`shutdown()` 及指标接口；调用失败通过 Ilias 任务结果和 `RpcError` 报告。详细用法目前以 [RPC 单元测试](../tests/unit/rpc/test_neko_rpc_backend.cpp) 为准。

Neko RPC 的 wire 版本及兼容列表由 Neko backend 定义，不是通用 RPC 框架给消息附加的字段；JSON-RPC 和自定义 backend 使用各自的协议约定。Neko RPC 当前普通帧以 `4E 52 01`（`NR`、wire 版本 1）开头，固定头的多字节整数按大端序编码，之后依次是方法标识、TLV 扩展和序列化负载。`NekoRpcFrameCodec::CompatibleVersions` 是本实现支持的 wire 版本列表，目前仅含 `1`；`codec` 区分负载序列化方式，方法表版本则是独立的扩展字段。Hello 负责方法 ID、压缩等能力协商，通用 RPC 核心不解释这些字段。

跨 wire 版本只固定一个最小约定：`UnsupportedVersion` 控制消息永远使用 V1 编码。它恰好是 24 字节：`4E 52 01 06`，随后 20 个零字节，即 V1 的 `kind=6`，其余头字段为零，没有方法、扩展或负载。识别它不依赖当前发送版本、兼容版本列表、codec、序列化器或压缩协商；客户端将其报告为 `RpcError::UnsupportedVersion`，服务端消费后不回复，避免循环。

头部可读取、完整帧长度一致且未超过资源限制，但 wire 版本不在兼容列表时，接收方回复上述固定消息，不解释该版本的消息类型或正文。头部无法识别、帧不完整或超过限制时不回复；流端点无法确定帧边界时终止该流。版本错误适用于 Hello 和普通消息，也适用于未启用 Hello 的连接。未来版本应保留这条 V1 错误消息的收发识别；普通协议如何扩展、是否继续兼容历史版本，到实现相应变更时再决定。目前不增加版本范围交换、自动降级或其他协商约定。

RPC 仍是实验性模块。`RpcBackend` concept、可替换压缩 codec、方法 ID 协商和公共头内的 `private/`、`detail/` 实现尚不构成稳定的第三方后端扩展承诺。本地 C++20 `test_neko_rpc_backend` 的 51 项测试覆盖多连接、取消、超时、关闭、负载限制、方法表更新、固定帧向量和版本不兼容通知；远端 Windows xmake MSVC、clang-cl 与 Linux C++20 常规测试已通过。CMake C++20 下 Linux/GCC 与 Windows/MSVC 的 RPC 构建、示例、安装包和源码消费者已验证 target、依赖链接和运行路径；Windows 使用 MSVC 构建的 Ilias 共享包。

## 职责边界

RPC 核心负责调用、分发、请求关联和内部任务的生命周期；网络与执行环境由 Ilias 和接入方管理。Endpoint 只需满足完整消息的 `recv`、`send`、`flush`、`shutdown`、`close` 接口，可以来自内存、UDP、自定义消息通道等。核心不假设 TCP、可靠投递、有序网络或特定帧头，不为 UDP 丢包自动重传；响应未到时，由调用方选择的 timeout、deadline 或取消策略结束等待。

Backend 负责消息格式、参数/返回值编解码、协议状态、协商和协议错误解释。核心保存并传递 backend context/session，不要求它们具有固定内部布局。流到完整消息的封装也是 backend 的可选能力；直接提供 `MessageEndpoint` 无须实现流工厂。

接入第三方 RPC 协议时，backend 应保持该协议的消息格式和交互规则；核心不添加统一帧头、版本字段、握手或必需扩展。NekoRPC 是本库自有、可替换序列化器的协议，只有它使用上述 Neko 帧头、Hello 和固定 V1 版本错误。`JsonRpcBackend` 的 readiness/control hook 不执行握手，消息使用 JSON-RPC 格式，对端无须使用本库。

HTTP 等传输差异沿用现有的自定义 stream/endpoint 适配机制；通用框架不据此增加协议字段或协商步骤。

## Backend 契约

`RpcBackend<B>` 检查通用操作及核心实际访问的消息结构，`RpcBackendFor<B, Method>` 在具体方法注册和调用处补查带方法类型的操作，不拿任意固定方法签名要求所有 backend 支持。

必需类型为 `Id`、`Message`、`ResponseValues`、`DecodedRequest`、`DecodeResult`、`EncodedRequest`、`Options`、`ClientContext`、`ServerContext`、`PeerSession`。`Id` 可默认构造、复制并按 `<` 排序，用于本地请求关联，不要求是整数或固定 wire ID。`Message` 是连续、每元素一字节的缓冲区，支持 `data/size/empty`；`ResponseValues` 支持默认构造、遍历和范围插入。`Options` 可默认构造和复制；`ServerContext` 与 `PeerSession` 可默认构造，并能接收工厂生成的新状态。

`EncodedRequest` 暴露 `message: Message`、`id: Id`。`DecodeResult` 暴露 `ok`、`batch`、可按索引访问的 `requests: DecodedRequest` 序列，以及 `responses: ResponseValues`。Backend 需保证解码结果及参数视图在分发完成前有效，生成的消息在发送完成前有效。

下表省略 `B::`；`bytes` 为 `std::span<const std::byte>`，`endpoint` 为核心的完整消息端点引用，`methods` 为 `std::vector<RpcMethodMetadata>`。

| 操作 | 返回值 |
| --- | --- |
| `makeClientContext(options)` | `ClientContext` |
| `makeServerContext(options, methods)` | `ServerContext` |
| `makeClientPeerSession(clientContext)` / `makeServerPeerSession(serverContext)` | `PeerSession` |
| `refreshMethodCatalog(serverContext, methods)` | `void` |
| `ensureClientReady(clientContext, session, endpoint)` | `ilias::IoTask<void>` |
| `handleServerControl(serverContext, session, endpoint, bytes)` | `ilias::IoTask<bool>`；`true` 表示已消费控制消息 |
| `decodeIncoming(serverContext, session, bytes)` | `DecodeResult` |
| `methodName(request)` / `id(request)` / `expectsResponse(request)` | 方法名、`const Id&`、`bool` |
| `appendError(responses, request, error)` | `void` |
| `encodeResponses(serverContext, session, responses, batch)` | `Message` |
| `clientNotInitError()` / `notificationOk()` | `std::error_code` |

不需要握手或控制消息的 backend 为对应必需 hook 提供无操作实现。工厂统一接收拥有字符串的公开方法元数据 `RpcMethodMetadata`，不要求 backend 理解 dispatcher 的内部方法存储。需要长期保存目录时，backend 应接收或复制其所有权，不保留指向临时目录的视图。

对具体 `Method`，还需实现以下模板操作；错误类型均为 `std::error_code`。解码参数类型由 backend 决定，核心将其交回同一 backend 的 `invoke`，不要求它等于方法声明的原始参数 tuple。例如 JSON-RPC 可以自动展开单个反射对象。

| 操作 | 返回值 |
| --- | --- |
| `encodeRequest<Method>(clientContext, session, method, notification, args...)` | `ilias::Result<EncodedRequest, error>` |
| `decodeResponse<Method>(clientContext, session, bytes, expectedId)` | `ilias::Result<Method::RawReturnType, error>` |
| `decodeParams<Method>(request, method)` | `ilias::Result<backend 解码参数类型, error>` |
| `invoke<Method>(method, params)` | `ilias::IoTask<Method::RawReturnType>` |
| `appendMethodReturn<Method>(responses, request, result)` | `void` |

上下文绑定还需 `invokeWithContext<Method>(function, requestContext, decodedParams) -> ilias::IoTask<Method::RawReturnType>`；仅在使用 `bindMethodWithContext` 时检查。参数如何展开由 backend 决定，核心不对 JSON 对象、tuple 或其他解码表示做特殊分支。

可选 hook 保持可选：

- `responseId(bytes) -> std::optional<Id>` 启用单接收任务和并发请求分流；启用时须提供 `handleClientControl(clientContext, session, bytes) -> bool`。没有该能力时，同一 endpoint 的请求收发串行执行。
- `handleClientControl(clientContext, session, endpoint, bytes) -> ilias::IoTask<bool>` 可在处理客户端控制消息时发送回复或返回协议错误；提供时优先使用此重载，串行收包也支持它。核心在调用期间持有发送锁。Neko backend 用它回复未知 wire 版本。
- `validateMessage(context, bytes) -> ilias::Result<void, error>` 由 backend 验证消息。
- `encodeCancel(id) -> Message`、`cancelId(bytes) -> std::optional<Id>` 提供协议级取消。
- `recoverClientCall(clientContext, session, endpoint, error, retryCount) -> ilias::IoTask<bool>` 决定协议级恢复；核心不推断错误含义，也不把它当作网络可靠性保证。
- `makeClientEndpoint`、`makeServerEndpoint` 或通用 `makeEndpoint` 可按接入角色提供；返回值满足 `MessageEndpoint` 即可。

## 执行环境和生命周期

`IoContext&` 是借用的执行环境，保留在构造接口中；上下文必须覆盖 RPC 对象、其内部任务，以及调用方仍在运行的 RPC 调用任务。推荐先构造 context，再构造 client/server，并按相反顺序销毁。传入 context 不等于在当前线程安装执行器；应按 Ilias 的要求先 `context.install()`，在负责这些任务的执行器上运行调用与收尾，直到任务清理完成后再卸载或销毁 context。核心不自动切换线程执行器；同一实例的调用和收尾应在同一个执行器上协调，收尾开始后不再发起新调用或替换端点。

- Client 的 context 覆盖整个 client 生命周期；每次替换 endpoint 创建/重置独立 peer session。Server context 覆盖整个 server 生命周期，每个接入端点各有 peer session。
- Client 内部接收/控制任务由 client 管理，`shutdown()` 会等待它们退出后释放作用域。Server 的端点任务、请求处理任务和取消清理由 server 管理。
- `RpcClient` / `RpcServer` 在销毁协议集合基类之前执行 `close()`，使内部任务不会在方法元数据已销毁后继续访问它。外部 `registerProtocol()` 借用的协议对象同样必须活到收尾结束。
- 返回给调用方的 `Task` 借用 RPC 对象及方法元数据，调用方需在销毁对象前完成或取消并等待这些任务。`close()` 会结束 pending 等待，但不接管调用方的 TaskGroup；不要保存未启动任务到对象销毁后再运行。
- `close()` 是同步强制收尾，`shutdown()` 是可等待的异步收尾；两者均应由外部拥有者调用。不要在本对象管理的 handler 中销毁自身，或等待包含当前 handler 的整个 server 退出。
