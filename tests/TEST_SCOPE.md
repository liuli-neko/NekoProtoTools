# Unit Test Scope

这份说明用来把覆盖率报告里的文件数字对应回测试意图。覆盖率入口仍然是 `build/coverage-report/index.html`，但补用例时建议先看这里的模块边界，再去 HTML 里找具体未覆盖行。

## JSON-RPC

- `tests/unit/jsonrpc/test_jsonrpc_protocol.cpp`
  - 模块：`include/nekoproto/jsonrpc/protocol.hpp`、`jsonrpc_traits.hpp` 的协议结构和模板分支。
  - 范围：请求对象的具名参数、位置参数、单反射对象自动展开、单 tuple 参数、空参数、可空参数缺省、必填参数缺省失败、请求 method 探测、成功响应、错误响应、void 响应。
  - 说明：这是纯序列化单测，不启动 client/server，不依赖 socket。
- `tests/unit/jsonrpc/test_jsonrpc.cpp`
  - 模块：JSON-RPC client/server/backend 的端到端流程。
  - 范围：方法绑定、动态调用、内置 rpc 方法、通知、批量调用、嵌套 API 名称、`rpc_no_prefix_tag`。
  - 说明：端点使用 `ilias::DuplexStream`，属于纯内存流，避免本地 UDP bind 权限影响单测结果。

## RPC

- `tests/unit/rpc/test_neko_rpc_backend.cpp`
  - 模块：通用 RPC 前端、二进制 backend、方法 ID 表、压缩 TLV、内存流端点。
  - 范围：注册方法调用、`DuplexStream` 端点、method id 协商和刷新、压缩/解压、通知无响应、内置方法、未找到方法错误。
  - 说明：这是 JSON-RPC 之外的 backend 验证，适合补 `include/nekoproto/rpc/**` 与二进制 RPC frame 行为。

## Serializer

- `tests/unit/serializer/test_json_backend.cpp`
  - 模块：RapidJSON 后端和通用 parser 分发。
  - 范围：基础类型、optional、variant、tuple、pair、sequence、map、指针、raw string、flat tag、缺字段策略、错误路径。
- `tests/unit/serializer/test_tags.cpp`
  - 模块：`serialization/private/tags.hpp`、`serializer_base.hpp` 的 `make_tags`/`NEKO_SERIALIZER` 元数据解析，以及 tags 进入 parser/schema 后的行为。
  - 范围：rename/comment/skipable/rawString/flat/unframed/fixedLength、递归 tag 消费、类型级 `is_flat_tag`/`is_unframed_tag`、JSON/Binary/schema 集成。
  - 用例分组：
    - `SerializationTags.*` 只验证 tag 元数据、宏解析、成员类型解析，不做真实 IO。
    - `SerializationTagIntegration.*` 验证 tag 经 JSON parser、schema generator、binary serializer 后的外部行为。
  - 重点边界：`comment_tag` 包住 `rename_tag` 时的递归解析、`NEKO_SERIALIZER(make_tags<...>)` 对字段名的提取、扁平化对象里的 renamed 字段是否出现在 schema、缺失 skipable renamed 字段是否保留默认值、类型级 flat/unframed tag 是否在 `NoTags{}` 下仍生效。
- `tests/unit/serializer/test_binary_serializer.cpp`
  - 模块：Binary reader/writer、固定长度、unframed 布局。
  - 范围：结构体、非字符串 map key、pair、嵌套 unframed、固定长度错误、截断输入。
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

## 常用命令

```bash
xmake f -m coverage --enable_tests=y --enable_jsonrpc=y --enable_rapidjson=y
xmake build test_jsonrpc_protocol
xmake run test_jsonrpc_protocol
xmake run coverage-report
```
