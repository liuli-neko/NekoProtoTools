# RPC Refactor Plan

本文档记录 JSON-RPC 模块向通用 RPC 前端与可扩展后端模型迁移的设计计划。

核心结论：RPC 的声明、注册、绑定、调用 API 不应绑定 JSON-RPC 2.0。JSON-RPC 应成为一个后端，负责 request/response 协议封装、编解码和对应的 message transport 适配。RPC 前端只关心方法元数据、协议集合、调用分发和通用通信端点。

Backend context/session 的完整边界见 [`rpc_backend_context_model.md`](rpc_backend_context_model.md)。本计划中的后端扩展、可选功能、协商、重试和 method-id 均以该模型为准：endpoint 只提供 complete-message transport；扩展只能由具体 backend 的 context/session 执行；RPC API 只保存和传递 opaque backend context。

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
- 创建和维护 backend-owned client/server context 与 peer session。
- 将 method metadata 和参数编码为 request。
- 将 request 解码为 method name、id、params payload。
- 将返回值或错误编码为 response。
- 将 response 解码为返回值或错误。
- 处理 batch、notification、协议版本字段、扩展协商、可选策略等后端语义。
- 在需要时提供 stream/datagram 到 complete-message 的 transport adapter，但 adapter 不执行协议扩展。

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

每个 backend 或应用层可以自己提供 transport wrapper。wrapper 只处理消息边界，不能承担 method-id、压缩、hello、重试等 backend context 职责。

```cpp
auto endpoint = JsonRpcBackend::makeEndpoint(std::move(stream));
server.addEndpoint(std::move(endpoint));
client.setEndpoint(std::move(endpoint));
```

RPC 核心不提供 listener/user/session 管理。应用层负责 accept、鉴权、对端选择和连接生命周期，然后把已建立的 endpoint 交给 RPC 层；backend 负责为该 endpoint 建立自己的 peer session。应用层也可以直接使用 `processMessage()` 做单条消息处理，但 backend context 仍是协议状态的归属处。

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
};

struct B {
    RpcMethod<std::string(std::string), "xxx", "value"> xxx;
};

struct Api {
    A a;
    B b;
};

namespace NekoProto {
template <>
struct Meta<::A> {
    constexpr static auto value = Object("xxx", &::A::xxx);
};

template <>
struct Meta<::B> {
    constexpr static auto value = Object("xxx", &::B::xxx);
};

template <>
struct Meta<::Api> {
    constexpr static auto value = Object("a", &::Api::a, "b", &::Api::b);
};
} // namespace NekoProto

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

当前定义：

```cpp
namespace tag_detail {
template <ConstexprString Prefix>
struct rpc_prefix_impl {
    constexpr static auto prefix = Prefix;
};

struct rpc_no_prefix_impl {
    constexpr static bool no_prefix = true;
};
} // namespace tag_detail

template <ConstexprString Prefix>
inline constexpr auto rpc_prefix = tag_detail::rpc_prefix_impl<Prefix>{};

inline constexpr auto rpc_no_prefix = tag_detail::rpc_no_prefix_impl{};
```

使用示例：

```cpp
struct Api {
    A a;
    B b;
    Common common;
    Legacy legacy;
};

namespace NekoProto {
template <>
struct Meta<::Api> {
    constexpr static auto value =
        Object("a", &::Api::a,                                      // 默认前缀 "a"
               "b", make_tags<rpc_prefix<"service.b">>(&::Api::b),
               "common", make_tags<rpc_no_prefix>(&::Api::common), // 不加前缀，但仍通过 client->common.xxx 调用
               "legacy", make_tags<rpc_no_prefix>(&::Api::legacy)); // 不加前缀，但仍通过 client->legacy.xxx 调用
};
} // namespace NekoProto
```

建议语义：

- 无 tag：如果 protocol 是集合字段，默认使用字段名作为前缀。
- `rpc_prefix<"x">`：使用自定义前缀 `x`。
- `rpc_no_prefix`：不拼接前缀，远端方法名保持 method name。
- 不设计 `rpc_global_tag` 作为稳定 API。field tag 只能改变注册出的远端方法名，不应承诺把嵌套字段提升成 C++ root 成员。

例如：

```cpp
struct Common {
    RpcMethod<std::string(), "version"> version;
};

struct Api {
    Common common;
};

namespace NekoProto {
template <>
struct Meta<::Common> {
    constexpr static auto value = Object("version", &::Common::version);
};

template <>
struct Meta<::Api> {
    constexpr static auto value =
        Object("common", make_tags<rpc_no_prefix>(&::Api::common));
};
} // namespace NekoProto

auto v = co_await client->common.version();              // 发送远端方法名 "version"
auto v2 = co_await client.callRemote<std::string>("version");
```

冲突规则：

- 完整方法名相同视为冲突。
- `rpc_no_prefix` 更容易冲突，应在注册期报错。
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
    decodeParams(const DecodedRequest& request, const Method& method);

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
    std::string signature;
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
};

namespace NekoProto {
template <>
struct Meta<::Api> {
    constexpr static auto value =
        Object("a", &::Api::a,
               "b", make_tags<rpc_prefix<"bee">>(&::Api::b));
};
} // namespace NekoProto
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
};

namespace NekoProto {
template <>
struct Meta<::Api> {
    constexpr static auto value =
        Object("system", make_tags<rpc_no_prefix>(&::Api::system));
};
} // namespace NekoProto
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

## RpcMethod 注册 API 现状

当前注册入口可以按“方法名从哪里来”分成四类：

- `RpcMethod<Signature, MethodName, ArgNames...>`：协议 struct 里的静态字段声明，`MethodName` 和参数名都在类型上；经过 `Reflect<T>` 展开后由 `RpcDispatcher::registerRpcMethod()` 注册。
- `RpcMethodF<Ptr, ArgNames...>`：协议 struct 里的函数指针声明，方法名由 `detail::func_nameof<Ptr>` 推导；`RpcServer::bindMethod<Ptr>()` 也复用同一套命名规则。
- `RpcMethodFN<Ptr, Owner, ArgNames...>`：函数指针声明，并把 `class_nameof<Owner>` 拼进方法名，适合把成员函数按类型命名空间暴露。
- `RpcMethodDynamic<Signature, ArgNames...>`：运行时方法名声明，由 `RpcServer::bindMethod(name, func)`、`RpcClient::callRemote(name, ...)` 和 `notifyRemote(name, ...)` 临时构造。

它们最终都会落到同一组基础元数据：

- `name()` / `declaredName()`：当前远端完整名和原始声明名。
- `RawReturnType` / `RawParamsType` / `ArgNamesHelper`：返回值、参数类型和参数名。
- `signature`：自动拼出来的字符串签名，例如 `int add(int lhs, int rhs)`。
- `description`：`rpc_desc` 或反射 metadata 提供的用户说明。
- `isBind()`：server 侧是否已经绑定实现。

当前短板是：后端 method-id 表、内建 introspection 和调试展示仍主要依赖 `signature` 字符串，无法稳定提取参数 schema、注释、分组、版本、废弃状态等结构化属性。

## RPC 结构化元数据计划

目标是让 RPC 前端把“用户声明的事实”保留下来，后端再通过统一收集层把 tag、反射元数据和类型信息合并成结构化 `RpcMethodInfo`。这样内建 introspection、method-id 签名、文档生成、客户端自动补全和 backend 专属扩展都使用同一份元数据。

新增声明式 API 时不要复用 `RpcMethod` 这个名字。`RpcMethod<Signature, "name", "arg"...>` 保持简洁风格；结构化属性走独立的 `RpcMethodSpec`：

```cpp
struct Api {
    RpcMethodSpec<
        int(int, int),
        rpc_name<"add">,
        rpc_desc<"Add two integers">,
        rpc_args<"lhs", "rhs">,
        rpc_version<"1.0.0">
    > add;

    NEKO_SERIALIZER(
        (make_tags<
            rpc_name<"math.add">,
            rpc_desc<"Override from reflection metadata">,
            rpc_args<"left", "right">
        >(add)))
};
```

合并规则：

- `RpcMethodSpec` 里的 `rpc_name/rpc_desc/rpc_args/rpc_version/rpc_notification` 提供声明默认值。
- 结构体反射 metadata 里的同名 tags 优先级更高，可以覆盖声明默认值。
- 名字优先级固定为：field tag 的 `rpc_name` > `RpcMethodSpec` 的 `rpc_name` / `RpcMethod<..., "name">` 声明名 > `Reflect<T>` 展开的字段名。
- `rpc_name` 只描述方法名段，可以包含业务允许的任意合法 RPC 名字；`rpc_prefix` / `rpc_no_prefix` 仍然按前缀规则独立参与最终远端名拼接。
- 前缀优先级固定为：field tag 的 `rpc_prefix/rpc_no_prefix` > `RpcMethodSpec` 的 `rpc_prefix/rpc_no_prefix` > 反射路径默认前缀。
- `rpc_args` 写在 `RpcMethodSpec` 里时参与编译期参数名和 JSON named params；写在 field tags 里时先覆盖收集到的元数据和展示字符串，不改变已经实例化的后端模板类型。
- 旧的 `RpcMethod`、`RpcMethodF`、`RpcMethodFN`、`bindMethod` 公共 API 不删除、不改签名。

建议元数据模型：

```cpp
struct RpcParamInfo {
    std::string name;
    std::string typeName;
    std::string summary;
    bool optional = false;
    // 后续可挂 schema、默认值、范围、枚举值、serializer tags 摘要等。
};

struct RpcMethodInfo {
    std::string name;
    std::string declaredName;
    std::string returnTypeName;
    std::string summary;
    std::string group;
    bool deprecated = false;
    bool notification = false;
    bool isBind = false;
    std::vector<RpcParamInfo> params;
};
```

收集层建议放在 `include/nekoproto/rpc/metadata.hpp`，职责如下：

- 从 `RpcMethodDynamic/RpcMethod/RpcMethodF/RpcMethodFN` 提取方法名、返回类型、参数类型、参数名和 notification 状态。
- 从协议字段 tags 提取 RPC 专属元数据，例如 summary/group/deprecated/version/param tags。
- 从参数和返回类型的 `Reflect<T>` 元数据提取字段名、`comment_tag`、`rename_tag`、`JsonTag{.skippable}`、`BinaryTag{.fixed_length}`、枚举值等可用于补全和 schema 的信息。
- 继续生成 `signature` 字符串，供当前 `rpc.get_method_info` 和 method-id 表使用。
- 生成 backend 可消费的精简签名数据；`NekoRpcMethodIdTable::signatureHash()` 后续应优先使用结构化签名，字符串 `signature` 只作为临时 fallback。

Dispatcher/Server 侧迁移路径：

- `RpcMethodWrapperBase` 新增 `metadata()` 或 `methodInfo()`，返回稳定存储的 `RpcMethodInfo`/view。
- `RpcDispatcher::MethodData` 从 `name/signature/description/isBind` 继续扩展为结构化信息。
- `RpcServer::_methodMetadata()` 不再手写 `name/description/isBind`，改为调用收集层。
- Backend context/session 的 method catalog refresh 从 `std::vector<RpcMethodMetadata>` 迁移到结构化 `std::vector<RpcMethodInfo>`，不保留无实际用途的兼容别名。

内建 introspection 迁移路径：

- 保持 `rpc.get_method_list`、`rpc.get_method_info_list`、`rpc.get_method_info` 的字符串行为。
- 新增 `rpc.get_method_metadata_list` / `rpc.get_method_metadata`，返回结构化信息。
- JSON-RPC backend 可以直接把结构化信息序列化为 JSON；二进制 backend 使用同一份 serializer 元数据。

推荐实施顺序：

1. 新增 RPC metadata tags 和 `metadata.hpp`，只做编译期/运行期收集，不改变注册行为。
2. 给现有四类注册 API 补测试：静态字段、函数指针字段、带 owner 的函数指针字段、动态 bind/call 都能生成一致 `RpcMethodInfo`。
3. Dispatcher 内部改存结构化元数据，但保持 `methodDatas()` 返回旧 view，避免一次性波及后端。
4. Backend context/session 的 method catalog refresh 改收结构化 metadata，并让 method-id signature hash 使用结构化签名。
5. 增加结构化 introspection 内建方法，再逐步让文档/补全工具消费新接口。

## RPC 属性收集重构 Checklist

重构目标从“多放一层 metadata”调整为“减少模板展开面、统一属性来源、删除无用过渡符号”。主要公共 API 保持稳定，但内部临时 API 不保留兼容层。

- [x] 新增统一字段属性 patch/collector：`rpc_visit_field` 不再分别从 method 和 reflectable 分支读取 tags，而是先把 field tags 收集成同一份 RPC 属性输入。
- [x] field tags 优先级固定高于 method 自身元数据：注册遍历时把 tags 作为外部覆盖输入传给 method；method 自身只暴露默认属性和应用 patch 的入口。
- [x] 删除旧的平行 tag 应用路径：移除旧的 tags 直写 method 流程，不保留内部转发壳。
- [ ] 将名字和前缀解析收敛到单一 resolver：名字优先级仍为 field tag > method 声明 > reflect field name；前缀优先级仍为 field tag > method spec > traversal prefix。
- [ ] 将 `rpc_args` 的 tag 提取从 `std::vector<std::string_view>` 迁到 view/span 形态，避免每次 tag 查询产生不必要临时容器。
- [ ] 将 method metadata storage 从按 `MethodTraits, ArgNames...` 模板实例化的大对象收敛为非模板存储；模板层只保留类型签名和调用包装。
- [ ] 将 `RpcMethodSpec` 的多轮 pack 扫描合并到单个 `RpcMethodSpecTraits<Specs...>`，减少重复 `requires`/fold 实例化。
- [ ] 将 dispatcher/server/backend 的 method metadata 获取改为统一收集层，避免每个后端重新拼字段。
- [ ] 每完成一步都扫描旧符号，不保留未使用别名、旧名字、转发壳或“兼容但无人用”的内部 API。
- [ ] 每完成一步至少跑 `test_jsonrpc`；涉及 binary backend metadata 或 method-id 时同步跑 `test_neko_rpc_backend`。

短期风险控制：

- 先统一 field tag 属性来源，不改变 wire 行为。
- 再移动 merge/resolve 逻辑，确保每一步都有现有测试覆盖。
- 结构体自身 metadata 暂不强行并入 RPC metadata；遍历时以 field tags 作为外部输入，必要时后续再给 struct-level metadata 增加显式 collector。

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
- [x] 新增 `include/nekoproto/rpc/tags.hpp`，定义 `rpc_prefix`、`rpc_no_prefix`。
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
- [x] 新增 `rpc_no_prefix` 测试：嵌套 C++ 访问路径不变，但通过无前缀 remote name 调用。
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
