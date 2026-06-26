# RPC Refactor Plan

本文档记录 JSON-RPC 模块向通用 RPC 前端与可扩展后端模型迁移的设计计划。

核心结论：RPC 的声明、注册、绑定、调用 API 不应绑定 JSON-RPC 2.0。JSON-RPC 应成为一个后端，负责 request/response 协议封装、编解码和对应的 message transport 适配。RPC 前端只关心方法元数据、协议集合、调用分发和通用通信端点。

目标模板参数顺序固定为：

```cpp
RpcServer<Backend, ProtocolSets...>
RpcClient<Backend, ProtocolSets...>
```

其中 `Backend` 总是第一参数，后续参数是一个或多个协议集合。

## 当前问题

当前 `include/nekoproto/jsonrpc/jsonrpc.hpp` 同时承担了以下职责：

- JSON-RPC 协议对象：`JsonRpcIdType`、request、response、error response。
- RPC 方法前端：`RpcMethodDynamic`、`RpcMethod`、`RpcMethodF`、method traits。
- server/client API：`JsonRpcServer<ProtocolT>`、`JsonRpcClient<ProtocolT>`。
- 后端调度：`JsonRpcServerImp`、`RpcMethodWrapper`。
- JSON 编解码：直接依赖 `JsonSerializer::InputSerializer` / `OutputSerializer`。
- 传输适配：`message_stream_wrapper.hpp` 中维护 JSON-RPC 专属 TCP/UDP message stream。

主要耦合点：

- `RpcMethodDynamic` 暴露 `RequestType = JsonRpcRequest2<...>` 和 `ResponseType = JsonRpcResponse<...>`，导致 RPC 方法元数据绑定 JSON-RPC。
- `RpcMethodWrapper::call` 直接接收 `JsonSerializer::InputSerializer` 和 `JsonRpcResponseValues`，导致调度层绑定 JSON parser 和 JSON-RPC response shape。
- `traits::Serializable<T>` 固定使用 `JsonSerializer::Reader/Writer`，导致 RPC trait 无法跟随后端切换。
- `JsonRpcClient` 自己构造 request、递增 id、写 JSON、等待 response、解析 error，导致 client 前端和后端协议不可分离。
- `MessageStreamClient` 是 complete-message stream 概念，但实现放在 JSON-RPC 模块内，且没有以 `ilias::Stream` 模板复用方式表达。

## 目标边界

### RPC 前端层

RPC 前端层负责：

- 声明 RPC 方法：`RpcMethod<Signature, Name, ArgNames...>`。
- 声明函数指针方法：`RpcMethodF<Ptr, ArgNames...>`。
- 生成 method metadata：名字、参数类型、返回类型、参数名、是否 notification。
- `RpcMethod` 只保留用户注册函数的原始元数据和绑定入口，不生成 request/response 壳。
- 提供 server 绑定 API：`server->method = func`、`server.bindMethod(...)`。
- 提供 client 调用 API：`client->method(args...)`、`client.callRemote<Ret>(...)`。
- 展开多协议集合，处理路径前缀和冲突检测。

RPC 前端层不负责：

- request/response 的 wire shape 或带壳类型。
- JSON-RPC id 规则。
- JSON、binary 或其他格式编解码。
- TCP、UDP、stdio、file、pipe 的帧格式。

### Backend 层

Backend 负责：

- 定义协议 id、错误类型和错误映射。
- 将 method metadata 和参数编码为 request。
- 将 request 解码为 method name、id、params payload。
- 将返回值或错误编码为 response。
- 将 response 解码为返回值或错误。
- 处理 batch、notification、协议版本字段等后端语义。
- 提供该后端适用的 complete-message endpoint wrapper。

JSON-RPC 只是其中一个 backend：

```cpp
template <typename... Protocols>
using JsonRpcServer = RpcServer<JsonRpcBackend, Protocols...>;

template <typename... Protocols>
using JsonRpcClient = RpcClient<JsonRpcBackend, Protocols...>;
```

兼容期可继续保留旧形式：

```cpp
JsonRpcServer<Protocol> server(ctx);
JsonRpcClient<Protocol> client(ctx);
```

旧形式内部转发到：

```cpp
RpcServer<JsonRpcBackend, Protocol>
RpcClient<JsonRpcBackend, Protocol>
```

### 通信端点层

RPC 接口层只接受通用 complete-message endpoint：

```cpp
template <typename T>
concept RpcEndpoint = requires(T endpoint, std::vector<std::byte>& out, std::span<const std::byte> in) {
    { endpoint.recv(out) } -> std::same_as<ilias::IoTask<void>>;
    { endpoint.send(in) } -> std::same_as<ilias::IoTask<void>>;
    { endpoint.close() } -> std::same_as<void>;
};
```

RPC 层不关心 endpoint 后面是 socket、file、stdio、pipe 还是内存队列。

每个 backend 或应用层可以自己提供 transport wrapper：

```cpp
auto endpoint = JsonRpcBackend::makeEndpoint(std::move(stream));
server.addEndpoint(std::move(endpoint));
client.setEndpoint(std::move(endpoint));
```

RPC 核心不提供 listener/user/session 管理。应用层负责 accept、握手、鉴权、对端选择和连接生命周期，然后把已建立的 endpoint 交给 RPC 层；或者直接使用 `processMessage()` 做单条消息处理。

对于 stream-like 传输，backend 可以提供：

```cpp
template <ilias::Stream StreamT = ilias::DynStream>
class JsonRpcStreamEndpoint;
```

对于 UDP 或 datagram-like 传输，backend 可以提供独立 wrapper：

```cpp
template <typename DatagramT>
class JsonRpcDatagramEndpoint;
```

这样新增 socket/file/stdio/pipe 支持时，只需要新增对应 endpoint wrapper 或应用层通道代码，不需要更新 RPC 前端。

## 目标目录

建议目录拆分：

```text
include/nekoproto/rpc/
  rpc.hpp                  # 公共聚合头
  method.hpp               # RpcMethodDynamic/RpcMethod/RpcMethodF
  traits.hpp               # function traits、参数展开、Serializable concept 转后端化
  registry.hpp             # method registry、协议集合展开、前缀处理
  dispatcher.hpp           # 通用请求分发，不绑定 JSON
  client.hpp               # RpcClient<Backend, ProtocolSets...>
  server.hpp               # RpcServer<Backend, ProtocolSets...>
  endpoint.hpp             # RpcEndpoint 概念与 endpoint type erasure
  tags.hpp                 # RPC 协议集合 tags

include/nekoproto/jsonrpc/
  jsonrpc.hpp              # 兼容聚合头
  backend.hpp              # JsonRpcBackend
  protocol.hpp             # JSON-RPC request/response/error objects
  stream_endpoint.hpp      # JSON-RPC stream/datagram wrappers
  error.hpp                # JsonRpcError
```

`jsonrpc_traits.hpp` 中与通用 RPC 有关的部分迁移到 `rpc/traits.hpp`，与 JSON 编解码有关的部分留在 `jsonrpc/backend.hpp` 或 `jsonrpc/protocol.hpp`。

## 多协议集合

新的 RPC server/client 支持一个或多个协议集合，并直接继承这些 API struct：

```cpp
RpcServer<JsonRpcBackend, AdminApi, UserApi, FileApi> server(ctx);
RpcClient<JsonRpcBackend, AdminApi, UserApi, FileApi> client(ctx);
```

协议集合可以是：

- 直接的 protocol struct，顶层方法注册到全局 root。
- 一个反射集合 struct，内部包含多个 protocol field。
- 带 RPC tag 的 protocol field。
- 后续可扩展的 `rpc::Protocol<ProtocolT, Tags...>` 包装类型。

默认 C++ 访问路径与 API 类型自然一致：

- `client->method()`：顶层方法，远端名为 `method`。
- `client->a.b.c.method()`：嵌套字段路径，远端名默认为 `a.b.c.method`。

推荐以反射集合表达命名空间式前端：

```cpp
struct A {
    RpcMethod<int(int), "xxx", "value"> xxx;
    NEKO_SERIALIZER(xxx)
};

struct B {
    RpcMethod<std::string(std::string), "xxx", "value"> xxx;
    NEKO_SERIALIZER(xxx)
};

struct Api {
    A a;
    B b;

    NEKO_SERIALIZER(a, b)
};

RpcServer<JsonRpcBackend, Api> server(ctx);
RpcClient<JsonRpcBackend, Api> client(ctx);

server->a.xxx = [](int value) -> ilias::IoTask<int> {
    co_return value + 1;
};

server->b.xxx = [](std::string value) -> ilias::IoTask<std::string> {
    co_return value + "!";
};

auto x = co_await client->a.xxx(1);
auto y = co_await client->b.xxx("hello");
```

默认规则建议为：protocol field 的字段名成为远端方法前缀。因此上例注册出的完整方法名是：

```text
a.xxx
b.xxx
```

这样同名函数可以在不同前缀下拥有不同实现。

## RPC tags

RPC 复用现有 `make_tags<Tag>(field)` 风格，将注册策略描述为字段 metadata，而不是写死进 protocol 类型。

建议新增：

```cpp
template <ConstexprString Prefix, auto BaseTags = NoTags{}>
struct RpcPrefixTag {
    constexpr static auto prefix = Prefix;
    constexpr static auto base = BaseTags;
};

template <auto BaseTags = NoTags{}>
struct RpcNoPrefixTag {
    constexpr static bool noPrefix = true;
    constexpr static auto base = BaseTags;
};

template <ConstexprString Prefix, auto BaseTags = NoTags{}>
inline constexpr auto rpc_prefix_tag = RpcPrefixTag<Prefix, BaseTags>{};

inline constexpr auto rpc_no_prefix_tag = RpcNoPrefixTag<>{};
```

使用示例：

```cpp
struct Api {
    A a;
    B b;
    Common common;
    Legacy legacy;

    NEKO_SERIALIZER(
        a,                                      // 默认前缀 "a"
        make_tags<rpc_prefix_tag<"service.b">>(b),
        make_tags<rpc_no_prefix_tag>(common),   // 不加前缀，但仍通过 client->common.xxx 调用
        make_tags<rpc_no_prefix_tag>(legacy)    // 不加前缀，但仍通过 client->legacy.xxx 调用
    )
};
```

建议语义：

- 无 tag：如果 protocol 是集合字段，默认使用字段名作为前缀。
- `rpc_prefix_tag<"x">`：使用自定义前缀 `x`。
- `rpc_no_prefix_tag`：不拼接前缀，远端方法名保持 method name。
- 不设计 `rpc_global_tag` 作为稳定 API。field tag 只能改变注册出的远端方法名，不应承诺把嵌套字段提升成 C++ root 成员。

例如：

```cpp
struct Common {
    RpcMethod<std::string(), "version"> version;
    NEKO_SERIALIZER(version)
};

struct Api {
    Common common;
    NEKO_SERIALIZER(make_tags<rpc_no_prefix_tag>(common))
};

auto v = co_await client->common.version();              // 发送远端方法名 "version"
auto v2 = co_await client.callRemote<std::string>("version");
```

冲突规则：

- 完整方法名相同视为冲突。
- `rpc_no_prefix_tag` 更容易冲突，应在注册期报错。
- 初期可以运行时报错并拒绝后注册项；稳定后可尽量将静态协议集合的冲突提前到编译期。

## Backend concept 草案

Backend 需要提供至少以下能力。具体名字可在实现时微调，但边界应保持稳定：

```cpp
struct JsonRpcBackend {
    using Id = JsonRpcIdType;
    using Error = std::error_code;
    using Message = std::vector<std::byte>;

    template <typename T>
    static constexpr bool serializable = /* backend-specific serializer check */;

    static Id nextId(std::uint64_t& counter);
    static bool isNotification(const Id& id);

    static DecodeResult decodeIncoming(std::span<const std::byte> message);

    template <typename Method>
    using ParamsType = typename JsonRpcMethodTraits<typename Method::MethodTraits>::ParamsTupleType;

    template <typename Method>
    static Result<ParamsType<Method>, Error>
    decodeParams(const DecodedRequest& request);

    template <typename Method, typename... Args>
    static Result<EncodedRequest, Error>
    encodeRequest(const Method& method, std::uint64_t& counter, Args&&... args);

    template <typename Method>
    static IoTask<typename Method::RawReturnType>
    invoke(Method& method, ParamsType<Method> params);

    template <typename Method>
    static void appendMethodReturn(ResponseValues& responses,
                                   const DecodedRequest& request,
                                   Result<typename Method::RawReturnType, Error> result);

    template <typename Method>
    static Result<typename Method::RawReturnType, Error>
    decodeResponse(std::span<const std::byte> message, const Id& expectedId);
};
```

RPC dispatcher 只看到 decoded request：

```cpp
struct DecodedRequest {
    std::string_view methodName;
    Backend::Id id;
    Backend::ParamsView params;
    bool notification;
};
```

`ParamsView` 是后端自己的输入视图。JSON-RPC 可以使用 `JsonSerializer::JsonValue` 或 backend-local wrapper；binary 后端可以使用 byte span 或 reader cursor。

## RPC registry

通用 registry 负责从协议集合展开完整方法名：

```cpp
struct RegisteredMethod {
    std::string fullName;
    std::string displayName;
    std::string description;
    bool isBind;
    MethodInvoker invoker;
};
```

展开规则：

```text
Protocol field path + Rpc tags + RpcMethod method name -> fullName
```

示例：

```cpp
struct Api {
    A a;
    B b;
    NEKO_SERIALIZER(a, make_tags<rpc_prefix_tag<"bee">>(b))
};
```

生成：

```text
a.xxx
bee.xxx
```

无前缀注册示例：

```cpp
struct Api {
    SystemRpc system;
    NEKO_SERIALIZER(make_tags<rpc_no_prefix_tag>(system))
};
```

生成：

```text
rpc.get_method_list
rpc.get_method_info
...
```

前端暴露策略：

- 集合字段仍保留自然 C++ 访问路径：`client->a.xxx()`。
- tags 只影响远端完整方法名，不改变 C++ 成员访问路径。
- 如果同一个方法既需要嵌套访问又需要 root 直接访问，应在 root protocol 中显式声明 alias，或通过 `callRemote("xxx")` 动态调用，避免默认行为含糊。

## 通用 server/client 草案

```cpp
template <typename Backend, typename... ProtocolSets>
class RpcServer {
public:
    RpcServer(ilias::IoContext& ctx);

    auto operator->() noexcept -> RpcServer*;

    template <RpcEndpoint Endpoint>
    void addEndpoint(Endpoint endpoint);

    template <ConstexprString... ArgNames, typename Ret, typename... Args>
    void bindMethod(std::string_view name, traits::FunctionT<ilias::IoTask<Ret>(Args...)> func);

    template <auto Ptr, ConstexprString... ArgNames>
    void bindMethod();

    ilias::Task<Backend::Message> processMessage(std::span<const std::byte> message);
};

template <typename Backend, typename... ProtocolSets>
class RpcClient {
public:
    RpcClient(ilias::IoContext& ctx);

    auto operator->() noexcept -> RpcClient*;

    template <RpcEndpoint Endpoint>
    void setEndpoint(Endpoint endpoint);

    template <typename Ret, ConstexprString... ArgNames, typename... Args>
    ilias::IoTask<Ret> callRemote(std::string_view name, Args&&... args);

    template <auto Ptr, ConstexprString... ArgNames, typename... Args>
    ilias::IoTask<typename traits::function_traits<decltype(Ptr)>::return_type>
    callRemote(Args&&... args);
};
```

## JSON-RPC backend 保留行为

JSON-RPC backend 需要保持当前行为：

- 支持 JSON-RPC 2.0 request、notification、response、error response。
- 支持 batch request。
- 支持 positional params 和 named params。
- 支持单参数反射对象自动展开。
- 支持 `std::optional<Reflectable>` 缺失 params 时为空。
- 支持 method introspection：
  - `rpc.get_method_list`
  - `rpc.get_method_info_list`
  - `rpc.get_method_info`
  - `rpc.get_bind_method_list`
- 支持现有 error code：
  - `MethodNotBind`
  - `ParseError`
  - `InvalidRequest`
  - `MethodNotFound`
  - `InvalidParams`
  - `InternalError`
  - `ClientNotInit`
  - `ResponseIdNotMatch`

错误类型建议拆分为：

- 通用 `RpcError`：前端和 dispatcher 使用。
- `JsonRpcError`：JSON-RPC backend wire code 映射。

兼容期允许 `JsonRpcError` 继续作为 public error code，但通用 RPC 层不应依赖它。

## 迁移步骤

- [x] 新增 `include/nekoproto/rpc/traits.hpp`，迁移通用 function traits、`RpcMethodTraits` 原始元数据。
- [x] 新增 `include/nekoproto/rpc/method.hpp`，迁移 `RpcMethodDynamic`、`RpcMethod`、`RpcMethodF`，移除 `RequestType/ResponseType`。
- [x] 新增 `include/nekoproto/rpc/tags.hpp`，定义 `rpc_prefix_tag`、`rpc_no_prefix_tag`。
- [x] 新增 `include/nekoproto/rpc/registry.hpp`，实现协议集合展开和前缀解析。
- [x] 新增 `include/nekoproto/rpc/endpoint.hpp`，定义 `RpcEndpoint` 概念。
- [x] 新增 `include/nekoproto/rpc/dispatcher.hpp`，把 request dispatch 从 `JsonRpcServerImp` 中抽出来。
- [x] 新增 `include/nekoproto/jsonrpc/protocol.hpp`，迁移 JSON-RPC request/response/parser 特化。
- [x] 新增 `include/nekoproto/jsonrpc/backend.hpp`，实现 `JsonRpcBackend`。
- [ ] 新增 `include/nekoproto/jsonrpc/stream_endpoint.hpp`，将当前 `MessageStream<IliasDynStream>` 等变为 JSON-RPC backend endpoint。
- [x] 新增 `RpcServer<Backend, ProtocolSets...>` 和 `RpcClient<Backend, ProtocolSets...>`。
- [x] 将 `JsonRpcServer<Protocol>` / `JsonRpcClient<Protocol>` 改为别名到新 API。
- [x] 迁移 `tests/unit/jsonrpc/test_jsonrpc.cpp`，保留旧 API 测试，同时新增嵌套 API 测试。
- [ ] 新增多协议前缀测试：`client->a.xxx()` 和 `client->b.xxx()` 调用同名方法但命中不同实现。
- [x] 新增 `rpc_no_prefix_tag` 测试：嵌套 C++ 访问路径不变，但通过无前缀 remote name 调用。
- [ ] 新增 endpoint 测试：使用内存 endpoint 或 pipe-like endpoint，不依赖 TCP/UDP。

## 兼容策略

短期保持以下 include 和类型可用：

```cpp
#include "nekoproto/jsonrpc/jsonrpc.hpp"

JsonRpcServer<Protocol> server(ctx);
JsonRpcClient<Protocol> client(ctx);
RpcMethod<int(int), "method"> method;
RpcMethodF<func> methodFunc;
```

新 API 推荐：

```cpp
#include "nekoproto/rpc/rpc.hpp"
#include "nekoproto/jsonrpc/backend.hpp"

RpcServer<JsonRpcBackend, Api> server(ctx);
RpcClient<JsonRpcBackend, Api> client(ctx);
```

旧 `detail::make_tcp_stream_client`、`detail::make_udp_stream_client` 可以先保留并转发到 `JsonRpcBackend` 的 endpoint factory。稳定后再移动到 `jsonrpc/stream_endpoint.hpp` 的 public namespace。

## 开放问题

- 如需 `client->version()` 这类 root 直接访问，应通过显式 root protocol/alias 或代码生成方案解决，普通 field tag 不承担成员提升。
- 多个 `ProtocolSets...` 直接传入时，默认 prefix 是类型名、空前缀，还是必须显式 tag。
- 静态冲突检测能覆盖多少场景。反射集合字段名和 `ConstexprString` prefix 可以编译期检查，动态 `bindMethod(name, ...)` 只能运行时检查。
- 后端 `Serializable<T>` 是否只检查 read/write 能力，还是也要检查 schema/introspection 能力。
- JSON-RPC batch 中多个请求是否继续共用一个 task group wait，还是改为 dispatcher 局部 scope。
