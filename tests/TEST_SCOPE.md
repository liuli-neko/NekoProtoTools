# Unit Test Scope

这份说明用来把覆盖率报告里的文件数字对应回测试意图。覆盖率入口仍然是 `build/coverage-report/index.html`，但补用例时建议先看这里的模块边界，再去 HTML 里找具体未覆盖行。

## JSON-RPC

- `tests/unit/jsonrpc/test_jsonrpc_protocol.cpp`
  - 模块：`include/nekoproto/jsonrpc/protocol.hpp`、`jsonrpc_traits.hpp` 的协议结构和模板分支。
  - 范围：请求对象的具名参数、位置参数、单反射对象自动展开、单 tuple 参数、空参数、可空参数缺省、必填参数缺省失败、请求 method 探测、成功响应、错误响应、void 响应。
  - 说明：这是纯序列化单测，不启动 client/server，不依赖 socket。
- `tests/unit/jsonrpc/test_jsonrpc.cpp`
  - 模块：JSON-RPC client/server/backend 的端到端流程。
  - 范围：方法绑定、动态调用、内置 rpc 方法、通知、批量调用、嵌套 API 名称、`rpc_no_prefix`。
  - 说明：端点使用 `ilias::DuplexStream`，属于纯内存流，避免本地 UDP bind 权限影响单测结果。

## RPC

- `tests/unit/rpc/test_neko_rpc_backend.cpp`
  - 模块：通用 RPC 前端、二进制 backend、方法 ID 表、压缩 TLV、内存流端点。
  - 范围：注册方法调用、`DuplexStream` 端点、method id 协商和刷新、压缩/解压、通知无响应、内置方法、未找到方法错误；还覆盖 call deadline/timeout/stop token、超时后的 pending 清理和 late response 隔离、跨连接全局 active/queued 背压、每连接和 client pending 上限、服务端排队加 handler 的总 timeout、惰性公开 `RpcRequestContext`、provider-supplied peer info、公开执行策略、连接级 timeout 设置、仅当前连接的任务/排队查询、指标以及 close/partial-frame 生命周期。
  - 边界矩阵：容量 `0`、恰好达到上限、上限外第一个请求；过去 deadline、`0ns`/负 timeout、固定种子普通随机参数和合法但非常规的 24 小时 timeout。connection timeout 另测 `0` 清除、`1ns`、中间值、server 上限减 `1ns`、等于/随机超过上限、`uint64_t` 极值和连接隔离。错误用例同时断言公开错误码、用户 handler 未被错误调用以及容量/指标可恢复。任务查询额外断言普通排队（无特权）、跨连接隔离、调用结束后为空（无历史）。
  - deadline 边界：客户端单次 deadline 是 local pending/cancel 契约；`rpc.set_connection_timeout` 提供 backend-neutral 的服务端可见默认值，正值严格小于 server timeout，并覆盖未来请求的 queue + handler。不通过 Neko backend 私有 extension 传播逐调用 deadline；未来若需要逐调用传播，必须设计公开且可协商的通用 request metadata。
  - 说明：这是 JSON-RPC 之外的 backend 验证，适合补 `include/nekoproto/rpc/**` 与二进制 RPC frame 行为。

## Serializer

- `tests/unit/serializer/test_json_backend.cpp`
  - 模块：RapidJSON 后端和通用 parser 分发。
  - 范围：基础类型、optional、variant、tuple、pair、sequence、map、指针、raw string、flat tag、缺字段策略、错误路径。
- `tests/unit/serializer/test_tags.cpp`
  - 模块：`global/reflection_tags.hpp`、`serializer_base.hpp` 的 `make_tags`/`NEKO_SERIALIZER` 元数据解析，以及 tags 进入 parser/schema 后的行为。
  - 范围：rename/comment/skippable/raw_string/flat/unframed/fixed_length、递归 tag 消费、类型级 `is_flat_tag`/`is_unframed_tag`、JSON/Binary/schema 集成。
  - 用例分组：
    - `SerializationTags.*` 只验证 tag 元数据、宏解析、成员类型解析，不做真实 IO。
    - `SerializationTagIntegration.*` 验证 tag 经 JSON parser、schema generator、binary serializer 后的外部行为。
  - 重点边界：`comment_tag` 包住 `rename_tag` 时的递归解析、`NEKO_SERIALIZER(make_tags<...>)` 对字段名的提取、扁平化对象里的 renamed 字段是否出现在 schema、缺失 skippable renamed 字段是否保留默认值、类型级 flat/unframed tag 是否在 `NoTags{}` 下仍生效。
- `tests/unit/serializer/test_binary_serializer.cpp`
  - 模块：Binary reader/writer、固定长度、unframed 布局。
  - 范围：结构体、非字符串 map key、pair、嵌套 unframed、固定长度错误、截断输入；独立输入/字符串/容器/对象/深度/分配预算，重复 map key、set element 和对象 wire field，null/string/variant golden vector，以及失败时目标值不变。
  - 边界矩阵：预算左边界、恰好上限、差一失败；固定种子中间随机值；随机非法 value tag 和每个截断前缀；合法但非常规的整数极值、无穷、NaN 和含 NUL/`0xff` 的字节串。
- `tests/unit/serializer/test_xml.cpp`
  - 模块：pugixml backend。
  - 范围：对象/数组/空容器/null、属性、文本内容、注释、modifier tag、解析错误。
- `tests/unit/serializer/test_to_string.cpp`
  - 模块：人类可读文本 writer。
  - 范围：根值、null、反射字段名。

## Proto And Reflection

- `tests/unit/proto/test_json_serializer.cpp`
  - 模块：协议对象经 JSON parser 的综合序列化。
  - 范围：容器、map/set、atomic、binary data、bitset、pair、指针、variant、optional、schema。
- `tests/unit/proto/test_proto.cpp`
  - 模块：`ProtoBase`、反射序列化、二进制协议、参数校验。
- `tests/unit/reflect/*.cpp`
  - 模块：静态反射元数据、对象/数组/单字段/const/optional 场景。

## Communication

- `tests/unit/communication/test_communication.cpp`
  - 模块：通信层帧校验、线程/切片/UDP 组合。
  - 说明：这组测试会创建本地 socket。若沙箱禁止 bind，需要单独提升权限运行；不要把这类失败当成协议序列化回归。

## 补覆盖率时的优先级

1. 先补纯内存、纯 parser 的测试，优先覆盖模板分支和错误路径。
2. 再补 `DuplexStream` 端到端测试，覆盖 client/server 合作逻辑。
3. 最后补真实 socket 或平台相关路径，并在测试名或文档里写清楚环境依赖。

## 用例设计约束

测试以用户可以观察的 API 契约和数据常识为准，不以当前实现的私有结构为准。每项输入能力至少检查：

1. 预期输入的左边界；
2. 固定种子生成的中间随机值；
3. 预期输入的右边界；
4. 随机错误值及公开错误反馈，并确认失败不污染输出状态；
5. 类型系统允许、但业务中不常见的任意合法值。

容量、超时和解析预算应同时检查“恰好允许”与“第一个不允许”的位置。随机测试必须可复现，并在失败信息中输出 sample 和输入值。

## Fuzz 与专项入口

- `test_json_proto`：JSON proto parser 的 libFuzzer 入口。
- `test_rpc_wire_fuzz`：统一覆盖 Binary Reader 的成功重编码不变量、Neko RPC frame、method-table TLV 和压缩解码；输入在 target 内先限长。
- Linux CI 分别提供 ASan、UBSan、TSan 的聚焦回归和两个 fuzz target 的 smoke run。普通单测仍是首要回归门槛，专项工具不替代 API 断言。

2026-07-15 最终复验：Debug 与 coverage 配置下的普通测试全集均为 25/25；coverage 为 lines 84.0%
（6087/7243）、functions 70.8%（27956/39465）。ASan 的 Binary 30/30、RPC 40/40，UBSan 的 Binary
30/30、RPC 40/40，TSan 的 RPC 40/40；两个 fuzz target 各完成 1000 次有界输入。受本地 ptrace 环境限制，
ASan/libFuzzer 运行禁用了 LeakSanitizer，地址与未定义行为插桩仍然启用。

## 常用命令

```bash
xmake f -m coverage --enable_tests=y --enable_jsonrpc=y --enable_rapidjson=y --ccache=n
xmake build test_binary_serializer
xmake run test_binary_serializer
xmake build test_neko_rpc_backend
xmake run test_neko_rpc_backend
xmake build test_jsonrpc_protocol
xmake run test_jsonrpc_protocol
xmake run coverage-report
```
