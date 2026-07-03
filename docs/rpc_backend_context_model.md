# RPC Backend Context Model

这份文档定义 RPC 层、backend 层、backend context/session、endpoint 之间的职责边界。目标不是给实现堆更多类型，而是把“谁能保存状态、谁能执行扩展、谁只负责转交数据”说清楚，避免默认后端继续把策略、传输、协议改写混在一起。

核心结论：

- RPC API 层可以帮助创建、保存、传递 backend context，但不能理解 context 内部内容。
- 所有协议扩展只能由具体 backend 实现执行，不能穿透到用户 API、dispatcher 或 transport endpoint。
- Endpoint 是 complete-message transport，只负责收发完整消息、关闭、刷新和停机。
- Backend context/session 是协议状态和扩展执行者，负责协商、编码、解码、策略状态和错误解释。

## 心智模型

把整个 RPC 调用链拆成四个平面：

| 平面 | 负责 | 不负责 | 防膨胀规则 |
| --- | --- | --- | --- |
| RPC API | 用户调用、绑定、方法目录、任务生命周期 | wire shape、扩展字段、压缩、method id、握手细节 | 只传递 opaque backend context，不读取、不分支、不修改 |
| Dispatcher | 将 logical request 路由到已绑定方法，收集 logical response | 网络、frame、TLV、扩展错误恢复策略 | 输入必须已经是可调度的方法名和参数视图 |
| Backend context/session | 协议编解码、连接期状态、扩展协商、策略状态、错误映射 | socket/file/UDP 读写、用户方法实现、跨 backend 通用 API | 每个扩展必须声明自己的状态归属和执行阶段 |
| Endpoint | complete-message 收发、close、flush、shutdown | 协议解析、扩展改写、method table、压缩、重试 | 不新增 backend 专属方法；不要求用户 endpoint 支持扩展 |

一句话判断某个逻辑应该放哪里：

- 需要理解 wire 协议或扩展 TLV：放 backend。
- 需要持有某个对端的协商状态：放 backend session。
- 只是收发一条完整消息：放 endpoint。
- 只关心 method name、参数、返回值：放 RPC API 或 dispatcher。
- 会改变用户可见调用形态：谨慎，默认不进入通用 API。

## 分层边界

### RPC API 层

RPC API 层负责让用户自然表达“我要调用哪个方法、传什么参数、是否通知、不等响应”。它可以知道当前 client/server 使用的是哪个 backend 类型，也可以保存 backend 给它的 context 对象。

RPC API 层不能知道：

- 这个 backend 是否启用了 method id。
- 当前对端 method table 是哪个版本。
- payload 是否会被压缩。
- 某个错误是否需要刷新表、重试或降级。
- hello、TLV、frame flag 的任何细节。

API 层允许做的事情只有：

- 在构造 client/server 时让 backend 创建 context。
- 在添加 endpoint 时让 backend 创建 peer session。
- 在每次调用时把 method metadata、参数、notification 标志交给 backend。
- 在收到 endpoint 消息后把消息交给 backend 解码。
- 把 backend 产出的 logical request 交给 dispatcher。
- 把 backend 产出的 error 或 value 原样交给用户。

API 层不应增加 method-id、compression、trace、auth 这类专用参数。即使以后有通用 retry policy，也应是通用 call policy，而不是某个扩展的特例。

### Dispatcher 层

Dispatcher 的输入应该是“已经解开的 RPC 语义”，不是 wire frame。

Dispatcher 只关心：

- method name。
- request id 或 backend 可比较的 correlation id。
- 是否期待 response。
- 参数 payload 或 backend 提供的参数视图。
- response 收集容器。

Dispatcher 不关心：

- 这个 method name 是从字符串来的，还是从 method id 还原来的。
- payload 是否曾经压缩过。
- 当前连接协商了哪些能力。
- 解码失败时客户端下一步是否要重试。

这样 method-id 失效就是 backend 解码阶段的错误，不会变成 dispatcher 的特殊分支。

### Backend Context

Backend context 是 backend 的协议执行环境。它可以拆成几个粒度，但这些粒度都是 backend-owned，RPC API 只负责携带。

| 状态粒度 | 生命周期 | 示例内容 | 使用规则 |
| --- | --- | --- | --- |
| Backend options | 用户配置 backend 时给出 | 是否允许 method id、压缩阈值、codec policy、统计收集器 | 只在 backend 创建 context 时读取，不传播到 wire 成为对端模式 |
| Client context | 一个 RpcClient 生命周期 | 本端 request id 分配器、默认策略、全局统计 | API 持有，但不检查字段 |
| Server context | 一个 RpcServer 生命周期 | 方法目录快照、服务端 feature policy、method id 分配器 | 方法绑定变化时由 API 通知 backend 刷新目录 |
| Peer session | 一个 endpoint/对端连接生命周期 | hello 状态、协商结果、对端能力、method table view、压缩选择 | 每个 endpoint 一份，不属于 endpoint 本身 |
| Call context | 一次调用生命周期 | correlation id、attempt 信息、调用级诊断、是否使用某扩展 | 只由 backend 读写；API 只是把它随调用带过 |

上下文传递的关键原则：

- RPC API 可以拥有 context 的对象生命周期。
- RPC API 不访问 context 字段。
- Backend 可以自由改变 context 内部布局，不影响 RPC API。
- 单 backend 的实验性状态不进入通用 concept。
- 多 backend 都需要的能力，先抽象成通用概念，再考虑放进 API。

### Endpoint

Endpoint 的职责应保持非常窄：收一条完整消息、发一条完整消息、flush、shutdown、close。

Endpoint 不应该：

- 读取或写入 method-id 表。
- 解压或压缩 payload。
- 解析 frame header。
- 吸收 hello。
- 自动重试。
- 暴露 refresh method table 之类 backend 专属方法。
- 要求用户实现“已经集成所有扩展”的自定义 endpoint。

Backend 可以提供 stream/datagram 到 complete-message 的 adapter，但这种 adapter 只能处理消息边界，例如 length prefix、datagram chunk、buffer 聚合。只要它开始理解 backend 扩展，它就已经不是 endpoint，而是 backend session 逻辑。

## 调用生命周期

### Client 发起调用

一次普通调用应按以下语义流动：

1. 用户调用 RPC 方法。
2. RPC client 收集 method metadata、参数和 notification 标志。
3. RPC client 创建或复用本次调用的 opaque call context。
4. Backend client context 根据当前 peer session 和 call context 编码 request。
5. Endpoint 只发送 backend 生成的完整消息。
6. 如果需要 response，endpoint 收到完整消息后交回 RPC client。
7. Backend client context 解码 response，更新自己的状态或产出错误。
8. RPC client 把值或错误交给用户。

在这条链上，method-id、压缩、trace、auth、retry 都不能让 RPC client 出现扩展专属分支。RPC client 只知道它正在让 backend 处理一条调用。

### Server 处理请求

服务端处理应按以下语义流动：

1. Endpoint 收到完整消息。
2. RPC server 把消息和对应 peer session 交给 backend server context。
3. Backend 解 frame、处理扩展、校验错误、产出 logical request 或 backend error。
4. Logical request 进入 dispatcher。
5. Dispatcher 调用用户绑定方法，收集 logical response。
6. Backend 根据 server context 和 peer session 编码 response。
7. Endpoint 只发送完整 response 消息。

服务端发现 method-id 失效时，只返回 backend error。服务端不应该替客户端刷新、重试或回退 name 请求。刷新、重试、降级都是客户端策略。

### 方法目录变化

Server 绑定、解绑或重新绑定方法时，RPC server 可以把新的 method catalog 通知 backend server context。这个 catalog 是普通 RPC 元数据，不是扩展字段。

Backend server context 可以选择：

- 更新 method-id 表。
- 标记已有 peer session 的表版本过期。
- 在后续 hello、显式刷新请求或正常响应附带提示中暴露新状态。
- 什么都不做，直到下一次连接协商。

RPC server 不关心 backend 选了哪种策略，只负责告知“方法目录变了”。

## 扩展模型

每个 backend 扩展必须回答五个问题，回答不清就不能合入默认后端：

| 问题 | 必须写清的内容 |
| --- | --- |
| 状态归属 | 状态属于 options、client context、server context、peer session 还是 call context |
| Wire 影响 | 会改变 frame、header、TLV、payload、错误格式还是只改变本地策略 |
| 执行阶段 | 在握手、request encode、request decode、response encode、response decode、错误观察中的哪个阶段执行 |
| 失败语义 | 失败返回什么 backend error，是否影响当前调用，是否影响后续调用 |
| 积累边界 | 需要新增字段、分支、统计或配置时，如何避免污染 API、endpoint、dispatcher |

扩展执行阶段建议保持固定：

| 阶段 | 可做的事 | 不可做的事 |
| --- | --- | --- |
| 配置阶段 | 读取 backend options，建立本端 policy | 向 RPC API 增加扩展参数 |
| 连接阶段 | 创建 peer session，协商对端能力 | 要求 endpoint 理解协商 |
| 请求编码 | 根据 method metadata、call context、peer session 生成 wire request | 让 API 传递扩展字段 |
| 请求解码 | 从 wire request 还原 logical request 或 backend error | 把半解码 frame 交给 dispatcher |
| 响应编码 | 根据 logical response 和 peer session 生成 wire response | 在 dispatcher 里拼 frame |
| 响应解码 | 还原调用结果或 backend error，更新 backend 状态 | 自动做扩展特例重试，除非明确配置了通用策略 |
| 观测阶段 | 记录统计、日志、诊断 | 改变调用语义 |

### Method ID

Method ID 是 backend 的 wire-level 优化，不是 RPC 方法元数据的一部分。

状态归属：

- 服务端 method-id 分配器属于 server context。
- 对端协商状态和当前表视图属于 peer session。
- 某次调用是否使用 method id 属于 call context。

执行规则：

- 请求编码时，客户端 backend 根据 peer session 决定发 name 还是 id。
- 请求解码时，服务端 backend 根据 peer session 把 id 解析成 logical method name。
- 解析失败时，服务端 backend 返回明确错误。
- 服务端不做自动刷新和自动重试。
- 客户端收到错误后，client context 可以标记表过期，也可以把错误交给用户。
- 是否刷新 method table、是否重试、是否降级 name 请求，是客户端策略，不是服务端或 endpoint 策略。

默认策略建议保守：

- 没有有效表时发 name。
- 表明确有效时可以发 id。
- 收到 table outdated、id not found、signature mismatch 等错误时，把表标记为 stale。
- 下一次调用可以选择 name，或在明确配置下先刷新表。
- 默认不透明重试已经到达服务端业务逻辑的调用。

### Compression

Compression 是 backend 的 payload 变换，不是 endpoint 的消息边界能力。

状态归属：

- 压缩算法选择和阈值属于 peer session。
- 压缩统计属于 backend options 或 backend context 持有的观测对象。
- 某次 payload 是否被压缩属于 call context 或 frame metadata。

执行规则：

- 请求或响应编码阶段决定是否压缩 payload。
- 请求或响应解码阶段检查协商结果并解压 payload。
- Endpoint 不参与压缩。
- 压缩失败是 backend error。
- 压缩统计只记录事实，不改变策略，除非明确有独立的 adaptive policy。

压缩扩展不能假设 method-id 存在。扩展之间的顺序由 backend 明确写在自己的处理主线里，不能靠 endpoint wrapper 顺序隐式决定。

### Auth、Trace、Metrics、Retry 等未来扩展

这些能力也遵守同一模型：

- Auth 可以是 backend 扩展或上层 middleware，但如果写入 wire metadata，就由 backend context 执行。
- Trace 可以通过 call context 传递关联信息，但 wire 表达由 backend 决定。
- Metrics 可以观察 API 调用和 backend 编解码，但不能修改请求。
- Retry 是客户端 call policy，不是 server error handling，也不是 endpoint 行为。

Retry 尤其要克制：

- 服务端只返回错误。
- 客户端根据策略决定是否重放。
- 已经进入服务端用户逻辑的调用，默认不透明重试。
- 某个扩展的错误不能偷偷触发专属 retry；要么交给用户，要么走统一 retry policy。

## 如何防止无限制积累

### 禁止往 endpoint 堆功能

Endpoint 的接口不为 backend 扩展增长。以下形态都应避免：

- 让 endpoint 暴露 method table 查询。
- 让 endpoint 暴露 refresh method list。
- 让 endpoint 暴露 compression stats。
- 让 endpoint 自动吸收 backend control frame。
- 让用户自定义 endpoint 时必须理解 backend 扩展。

如果需要这些能力，它们应该在 backend context/session 中出现。

### 禁止往 RpcClient/RpcServer 堆扩展分支

RPC API 只允许出现 backend-agnostic 的生命周期动作，例如创建 context、传递 context、通知方法目录变化、把 endpoint 关联到 peer session。

以下形态都应避免：

- API 判断某个 error 是否是 method-id 错误。
- API 决定 compression 是否启用。
- API 为某个 backend 的扩展增加参数。
- API 为某个 backend 的扩展增加 callback。
- API 因为一个 backend 的扩展修改通用 concept。

如果多个 backend 都需要同一种能力，先写成独立的通用模型，再决定是否进入 API。

### 禁止把所有策略塞进 Backend concept

通用 Backend concept 应只描述 RPC 必须能力：编码 request、解码 request、编码 response、解码 response、错误映射、可序列化性。单个 backend 的扩展不进入通用 concept。

Backend 专属扩展通过 backend context 的内部成员、backend options、backend-local helper 实现。RPC API 只通过少量稳定入口把 context 交还给 backend。

### 每个扩展必须有终止条件

扩展不能只说“以后可能有用”。每个扩展都要写清楚它不会继续膨胀到哪里：

- Method ID 不进入 `RpcMethod`。
- Compression 不进入 endpoint。
- Retry 不进入 server 自动处理。
- Auth 不进入 method 参数。
- Trace 不改变 method signature。
- Metrics 不影响 request/response 语义。

如果一个扩展需要新增第三个以上的跨层入口，说明边界可能错了，应先重新设计。

### 目录和命名也用于防膨胀

建议按职责组织文件，而不是按“哪个扩展最近加的”组织：

| 区域 | 内容 |
| --- | --- |
| rpc public | 用户 API、method、registry、dispatcher、endpoint concept、backend concept |
| rpc private | 通用 helper 和 backend 不应暴露的结构 |
| backend public | backend 类型、options、兼容别名、必要聚合头 |
| backend private | frame codec、extension codec、method table、compression codec、context/session 细节 |
| docs | 当前模型、默认后端契约、单扩展设计、迁移计划 |

命名规则：

- 用户 API 用稳定、短、无扩展细节的名字。
- Backend context 内部名字可以具体，但不能泄漏到通用 API。
- 同一概念只保留一个名字，不用兼容别名拖延迁移。
- 如果一个名字里出现 endpoint，但内容在做协议策略，应重命名为 session 或 context。

## 默认 Neko-RPC 的目标边界

默认 Neko-RPC backend 的目标不是做一个大而全的 RPC runtime，而是提供一条清晰、可扩展、能被其他 backend 参考的协议主线。

默认后端应该保留：

- 固定 frame header。
- TLV extension map。
- 可替换 serializer policy。
- 可选 method-id。
- 可选 compression。
- backend-local context/session。
- 清晰错误码。
- 基础统计。

默认后端不应该承担：

- 用户连接管理。
- listener/user/session manager。
- endpoint 专属扩展接口。
- 自动业务级 retry。
- 跨连接永久 method-id cache。
- 把所有未来扩展提前塞进公共 API。

## 设计检查清单

新增或修改 RPC/backend 功能前，先检查：

- 这是不是 backend 专属能力？如果是，不改通用 API。
- 它需要保存状态吗？状态归属是否明确？
- 它是否要求 endpoint 做协议解析？如果是，边界错了。
- 它是否要求 dispatcher 理解 wire 细节？如果是，边界错了。
- 它是否让用户方法签名出现协议扩展参数？如果是，边界错了。
- 它是否有默认不透明重试？如果是，先证明不会重复执行业务逻辑。
- 它是否需要新增通用 Backend concept 方法？如果只是一个 backend 需要，不能加。
- 它是否有观测需求？观测是否只读？
- 它是否有明确的失败错误码？
- 它是否写清楚了不会继续膨胀到哪里？

这份清单比具体实现更重要。实现可以调整，但一旦某个功能开始跨过这些边界，就应该先停下来重新定位。
