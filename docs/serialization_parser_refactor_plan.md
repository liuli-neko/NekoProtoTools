# Serialization Parser Refactor Plan

本文档记录 NekoProtoTools 序列化链路从旧 `save/load + Serializer CRTP` 模型迁移到
reflect-cpp 风格通用 Parser 模型的计划。

核心结论：Parser 必须是通用层，不属于 RapidJSON、simdjson、binary、xml 等任何具体协议后端。协议后端只提供薄的 Reader/Writer 能力，类型适配规则放在通用 Parser 或 `types/xxx.hpp` 的 Parser 特化里。

## 当前执行边界

本轮重构集中通用反射 Parser、现有 JSON 后端、binary 和 schema：

- RapidJSON、simdjson、binary 和 pugixml 接入同一 Parser 层。
- 保留 `JsonSerializer::OutputSerializer/InputSerializer`、`JsonSerializer::JsonValue`、`to_json_value` 等外部接口。
- 新增通用 Parser 代码不能包含 RapidJSON/simdjson/pugixml 头。
- RapidJSON 文件只保留后端能力和后端原生类型特化，不继续维护 `RapidJsonParser<T>` 类型清单。
- 输出状态路由必须是通用 Parser 层能力，后端只提供固定 Writer/Reader API 和底层 output/input handle 类型。
- 后端扩展不需要手写 `XxxBackend` 或 `ParserFormatOps`；Parser 直接以 `Reader, Writer` 为模板参数，后端特殊能力由 Reader/Writer 的可选 API 表达。
- 如果通用类型规则无法覆盖某个类型，或某后端确实需要特殊处理，显式新增 `Parser<Reader, Writer, T>` 特化；不要让所有类型都先经过后端专用 parser 再转发。
- 每次迁移一组类型后，用 JSON roundtrip 或输出形状测试确认旧行为未漂移。

## 可勾选任务列表

- [x] 明确当前边界：通用 Parser + RapidJSON + simdjson + binary + schema + XML。
- [x] 建立 `include/nekoproto/serialization/parsing/parser.hpp`，承载通用 Parser 主模板和 `parser_read/parser_write`。
- [x] 让 RapidJSON public serializer 入口调用通用 `parser_write/parser_read`，不直接依赖 `rapid_json_write/read`。
- [x] 保留 RapidJSON backend/native value 的后端特化，避免把 `RapidJsonValue` 放进通用类型规则。
- [x] 补最小 JSON 后端测试，锁定 primitive/string/optional/atomic/reflect 的新路径。
- [x] 迁移 basic/string/enum/null Parser 到通用层。
- [x] 迁移 sequence/map/tuple/pointer/variant Parser 到通用层。
- [x] 单独迁移 reflection Parser，并把 JSON-RPC 依赖的反射字段 helper 改成通用 Reader/Writer parser。
- [x] 迁移 bitset/byte/binary_data 等剩余小类型 Parser 到通用层。
- [x] 删除 `RapidJsonParser<T>` 过渡转接层，统一由 `parser_write/parser_read -> Parser<Reader, Writer, T>` 分发。
- [x] 将输出 root/object/array/member/element 状态路由统一到通用 `Parent<Writer>`，删除 RapidJSON 专属 OutputTarget/Object/Array。
- [x] 删除 `ParserBackend` / `RapidJsonBackend` / `ParserFormatOps` 胶水层。
- [x] 删除 `ParserRuntimeTags` / `ParseContext`，原始 tag 类型和值直接传入 Parser。
- [x] 删除 `json/rapid_json_parser.hpp`，通用 parser include 统一到 `parsing/parsers.hpp`。
- [x] 锁定 tag 边界：`flat/skippable` 属于反射字段，`raw_string` 属于字符串 Parser + JSON 能力，`fixed_length` 留给 binary Parser。
- [x] Parser 返回值统一为 `sa::Result<void>`，保留具体错误码、字段名和元素索引上下文。
- [x] 为缺失 required 字段、tuple 长度、JSON 语法和 raw JSON 错误补充错误码/message 测试。
- [x] 将 `test_json_serializer.cpp` 整体从旧状态机 API 迁移到 `operator()` roundtrip/JSON 形状断言。
- [x] simdjson 改为薄 Reader + 共享 JSON TextWriter，public serializer 直接调用通用 Parser。
- [x] binary 实现 stream-style Reader/Writer，保留旧 wire layout，并让 `fixed_length` tag 选择可选能力。
- [x] `fixed_length` 通过 `supports_fixed_length<Reader, Writer, T>` 检测完整读写 API 组，
  arithmetic Parser 不再内联后端 API 探测。
- [x] 删除 `FixedLengthField/make_fixed_length_field` 壳；binary 定长字段只从反射 tags 获取宽度。
- [x] 反射元数据从 value wrapper 迁移为独立字段 metadata；tags 只描述字段/调用边，
  不作为类型自己的永久属性。
- [x] `MessageHeader` 是通信层帧头，不注册进 `ProtoFactory`；通信层以调用点 tag 显式读写
  unframed header，避免把帧布局重新伪装成类型级协议属性。
- [x] binary 的 `unframed` 布局由 `supports_unframed_objects<Reader, Writer>` 检测完整 API 组后执行，
  后端不声明能力常量。
- [x] 删除 `parser_write/read_reflect_fields_unframed` 和 `MessageHeader` 的 binary `CustomParser`；
  无框架布局不再形成第二套反射遍历路径。
- [x] 提供显式 `CustomParser<Reader, Writer, T>` 逃生口，只用于真正的类型/后端特殊规则。
- [x] schema 类型遍历进入通用 Parser；`json/schema.hpp` 只负责转换为 Draft-07。
- [x] 实现真实 `IsSchemafulWriter` capability concept，不把 schema 生成和 schemaful Writer 混为一层。
- [x] 补 simdjson、binary、schema 结构与跨 RapidJSON/simdjson 后端测试。
- [x] XML 使用 pugixml；对象字段映射为子元素，数组映射为同名兄弟元素，`xml_content` 映射为节点文本。

## 参考模型

参考 reflect-cpp 的分层方式：

- 顶层格式 API 只暴露 `read/write` 或项目里的 `InputSerializer/OutputSerializer::operator()`。
- `Parser<Reader, Writer, T>` 是类型分发中心，不绑定某个具体格式。
- 每个格式实现自己的 Reader/Writer，Reader/Writer 是序列化库和 Parser 之间的薄层。
- Writer 提供 root、array、object、value、null、end-array、end-object 这一类构造能力。
- Reader 提供 `to_basic_type`、`to_array`、`to_object`、`get_field`、`read_array`、`read_object`、`is_empty` 这一类读取能力。
- 对 schemaful 格式，Reader/Writer 需要额外区分 object、map、union；这也是旧 API 对非 JSON 格式不友好的根源之一。
- reflect-cpp 的字段缺失规则位于 `NamedTupleParser`，optional/default/skip 等类型规则位于对应 Parser。
- YAS 等定序格式由 Reader 按位置提供全部字段，字段名和“缺失命名键”语义不会泄漏到 basic Parser。

本次实际对照的是本地 `/home/user/workspace/subroot/reflect-cpp/` 源码。

参考链接：

- reflect-cpp custom format: https://rfl.getml.com/supported_formats/supporting_your_own_format/
- reflect-cpp Parser base: https://github.com/getml/reflect-cpp/blob/main/include/rfl/parsing/Parser_base.hpp
- reflect-cpp JSON Reader/Writer: https://github.com/getml/reflect-cpp/tree/main/include/rfl/json

## 当前旧模型

当前旧链路大致是：

```cpp
OutputSerializer::operator()(value)
  -> prologue
  -> value.save(serializer) 或 save(serializer, value)
  -> serializer.startArray/startObject/saveValue/...
  -> epilogue

InputSerializer::operator()(value)
  -> prologue
  -> value.load(serializer) 或 load(serializer, value)
  -> serializer.startNode/name/...
  -> epilogue
```

主要文件：

- `include/nekoproto/serialization/private/helpers.hpp`
  保留 `NameValuePair`、`make_name_value_pair` 作为迁移期字段绑定参考。
  其中 `detail::OutputSerializer` / `detail::InputSerializer` 是旧 CRTP 分发层，最终删除。

- `include/nekoproto/serialization/private/traits.hpp`
  旧模型用于探测 `save/load` 成员函数和自由函数。
  迁移后只保留仍有价值的 type traits，例如 optional-like、minimal-serializable，或将其改名迁移到 parsing traits。

- `include/nekoproto/serialization/types/*.hpp`
  当前大部分容器、指针、tuple、variant、reflection 适配都写成自由函数 `save/load`。
  这些是主要重构对象。

- `include/nekoproto/serialization/json/rapid_json_reader.hpp`
  目标保留为 RapidJSON Reader 薄层。

- `include/nekoproto/serialization/json/rapid_json_writer.hpp`
  目标保留为 RapidJSON Writer 薄层。

- `include/nekoproto/serialization/json/rapid_json_serializer.hpp`
  目标保留 public serializer 兼容层，只暴露构造、`operator()`、`end()`、`operator bool()`。

## 必须保留的外部接口

迁移完成前后都应保留：

- `JsonSerializer::OutputSerializer out(buffer);`
- `out(value);`
- `out.end();`
- `JsonSerializer::InputSerializer in(data, size);`
- `in(value);`
- `if (in) { ... }`
- `JsonSerializer::JsonValue`
- `to_json_value(obj)`

对于具体序列化器，数据读写接口只保留 `operator()`。旧的 `startObject`、`startArray`、`startNode`、`saveValue`、`loadValue`、`name`、`rollbackItem` 不再作为新后端 public API。

迁移期间可以保留旧函数作为行为参考：

- `save(serializer, value)`
- `load(serializer, value)`
- `value.save(serializer)`
- `value.load(serializer)`

但它们不应成为新 RapidJSON 路径的核心分发机制。

## 旧行为基线

重构后默认应保持以下行为，除非测试和调用方明确改约定：

- arithmetic、bool：写为 basic value；有反射名称的 enum 优先写名称，否则写 underlying integer。
- `std::string` / `std::string_view` / `const char*`：写为字符串；`raw_string` tag 生效时写入原始 JSON 值。
- `std::vector<T>`、`std::list<T>`、`std::deque<T>`：写为数组。
- `std::array<T, N>`：写为数组，读取时 size 必须匹配或返回失败。
- `std::set<T>` / `std::unordered_set<T>` / multiset：写为数组。
- `std::pair<K,V>`：写为对象 `{ "first": ..., "second": ... }`。
- tuple：写为数组，按位置读写。
- `std::map<std::string,V>` / `std::unordered_map<std::string,V>`：写为对象。
- 非字符串 key 的 map/multimap/unordered_map：写为数组，每个元素是 `{ "key": ..., "value": ... }`。
- `std::optional<T>`：有值写 inner value，无值写 null；缺失时默认 reset。
- 字段带 `skippable` 时允许缺失且保留目标对象原值，不要求字段类型必须是 optional。
- `std::shared_ptr<T>` / `std::unique_ptr<T>`：空指针写 null，非空写 `*ptr`。
- `std::variant<Ts...>`：保持当前 variant 形状；迁移前需要用测试锁定具体 JSON 形状。
- `std::monostate`：写 null。
- `std::byte`、`std::bitset<N>`、`BinaryData<T>`：保持当前 JSON 表达方式；迁移前需要用测试锁定。
- 反射结构：
  如果有字段名，写 object。
  如果只有 values meta，没有 names meta，写 array。
- `NameValuePair<T>`：作为字段名和值的显式绑定。
- 序列大小信息：由各 backend reader 按能力直接暴露，通用 Parser 不再依赖额外 helper。

## 目标架构

建议目标目录：

```text
include/nekoproto/serialization/
  parsing/
    parser.hpp              # Parser 主模板和 parser_read/parser_write
    parsers.hpp             # 通用 Parser include 聚合
    concepts.hpp            # Reader/Writer 能力约束
    field.hpp               # NameValuePair/field name/field policy
    basic.hpp               # arithmetic/string/null/enum
    sequence.hpp            # vector/list/deque/array/set
    map.hpp                 # string-key map 与 non-string-key map
    tuple.hpp               # pair/tuple
    pointer.hpp             # shared_ptr/unique_ptr
    optional.hpp
    variant.hpp
    reflection.hpp
    binary_data.hpp
  json/
    rapid_json_reader.hpp   # RapidJSON Reader
    rapid_json_writer.hpp   # RapidJSON Writer
    rapid_json_serializer.hpp
  private/
    tags.hpp                # tag 定义可先保留在这里
```

目标核心模板：

```cpp
namespace nekoproto::detail {

template <typename Reader, typename Writer, typename T, typename Enable = void>
struct Parser;

template <typename Reader, typename Writer, typename T, typename Parent, typename Tags = NoTags>
ParserResult parser_write(Writer& writer, const T& value, const Parent& parent, const Tags& tags = {});

template <typename Reader, typename Writer, typename T, typename Tags = NoTags>
ParserResult parser_read(typename Reader::InputValueType in, T& value, const Tags& tags = {});

}
```

当前实现已删除 `Parser<Backend, T>` 过渡形态，类型分发直接使用 `Parser<Reader, Writer, T>`。

## Parser 错误边界

- Parser write/read 统一返回 `sa::Result<void>`，不再用 `bool` 丢失失败原因。
- `InvalidField` 表示 required 命名字段缺失，message 包含字段名。
- `InvalidLength` 表示 tuple、固定数组或 positional reflect 的元素数量不匹配，message 包含 expected/actual。
- `InvalidType` 保留 Reader 的具体类型错误，并由 reflection/tuple/map 逐层补字段名或元素索引。
- `ParseError` 表示 JSON 语法、raw JSON、base64 或 variant alternatives 等解析失败。
- public serializer 的 `operator()` 仍返回 `bool` 兼容旧调用；`error()` 返回最近一次失败的 `sa::Error`。

## Reader/Writer 能力边界

Writer 只负责格式构造，不负责 C++ 类型规则：

- `OutputArrayType`
- `OutputObjectType`
- `OutputVarType`
- `array_as_root(size)` 或项目风格 `arrayAsRoot(size)`
- `object_as_root(size)`
- `null_as_root()`
- `value_as_root(value)`
- `add_array_to_array(size, parent)`
- `add_array_to_object(name, size, parent)`
- `add_object_to_array(size, parent)`
- `add_object_to_object(name, size, parent)`
- `add_value_to_array(value, parent)`
- `add_value_to_object(name, value, parent)`
- `add_null_to_array(parent)`
- `add_null_to_object(name, parent)`
- `end_array(parent)`
- `end_object(parent)`

Reader 只负责格式读取，不负责 C++ 类型规则：

- `InputArrayType`
- `InputObjectType`
- `InputVarType`
- `is_empty(value)`
- `to_basic_type<T>(value)`
- `to_array(value)`
- `to_object(value)`
- `get_field_from_array(index, array)`
- `get_field_from_object(name, object)`
- `read_array(arrayReader, array)`
- `read_object(objectReader, object)`
- 可选：`use_custom_constructor<T>(value)`

schemaful 格式后续额外能力：

- map 与 object 分离。
- union 与 variant 分离。
- fixed-length/schema 信息不通过旧 `startNode + size helper` 模拟。

## Parser 层职责

Parser 层负责 C++ 类型规则：

- basic parser：arithmetic、bool、enum、string、nullptr。
- sequence parser：vector/list/deque/array/set/multiset/unordered_set。
- map parser：区分 string-key map 和 non-string-key map。
- tuple parser：pair 和 tuple。
- optional parser：空值、缺失字段、`skippable`。
- pointer parser：空指针和对象生命周期。
- variant parser：variant 形状、monostate、类型选择。
- reflection parser：反射字段遍历、字段名、缺失字段、flatten。
- tag parser：`TaggedField` 将调用点/字段 tags 传给被绑定的 value；反射元数据定义期拆出
  `name/accessor/tags`，不把 tags 存在 value wrapper 里。
- backend-native value parser：例如 `RapidJsonValue`，只在对应 backend 的 specialization 里实现。

Parser 层不应该：

- 直接包含 RapidJSON/simdjson/pugixml 的头，除 backend-native value specialization 外。
- 调用旧 serializer 的 `startObject/startArray/startNode`。
- 假设所有格式都是 JSON object/array。
- 要求新 Reader 必须通过额外 tag 对象暴露 size。

## Tag 正式生效计划

现有 tag：

- `JsonTag::flat`
- `JsonTag::skippable`
- `JsonTag::raw_string`
- `BinaryTag::fixed_length`
- `BinaryTag::unframed`

迁移目标：

- tags 的语义统一为“当前 value 在当前绑定位置上的元数据”。反射字段、外层成员、root 调用都可以给
  当前 value 传 tags；类型本身不因为自己的定义而永久携带 tags。
- tags 不是“按顺序嵌套的一串可消费 wrapper”，而是“当前绑定位置上的属性集合”。序列化层对 tag 的解释应当
  按 feature 独立判断，而不是依赖 `consume -> rebuild -> 继续递归` 的链式处理模型。
- C++26 注解接入时，注解解析层应直接生成字段 metadata：`name/accessor/tags`。这些 tags 与宏
  `make_tags<...>(field)` 生成的 tags 等价，不需要额外 wrapper。
- 宏生成的反射元数据使用 `Neko::field_tags`，手写 `Meta<T>` 也应使用 `field_tags` 表示字段 tags；
  `Neko::tags` / `Meta<T>::tags` 不再作为类型级 tags 入口。
- `make_tags<Tags>(accessor)` 表示“带 tags 的字段/调用绑定描述”，不要求后续 Parser 把字段值包成
  value wrapper 才能消费 tags。
- 如果一个 struct 作为另一个 struct 的成员被标了 tags，这些 tags 只作用于这个成员槽位；进入该
  struct 自己的字段遍历后，只使用其内部字段各自的 tags，不与外层 tags 合并。
- Reflection 将原始 `Tags` 对象直接传给 `Parser<Reader, Writer, T>`，不压缩为公共 runtime context。
- `TaggedField`、`optional<T>`、`shared_ptr<T>`、`unique_ptr<T>`、`variant<Ts...>` 这类“同一槽位的透明包装”
  可以继续把当前 tags 透传给其内部值；`struct/object`、`tuple/pair`、`array/sequence`、`map` 这类“进入子槽位”
  的结构边界不继续向子元素传播外层 tags。
- `skippable` 由 reflection/object-field parser 消费：命名字段缺失时成功并保留目标对象原值；显式 null 仍交给字段类型 Parser 判断。
- 未标记 `skippable` 的 optional 字段缺失时 reset；普通字段缺失时失败。
- 对 positional/定序格式，不存在“命名键缺失”；长度/读取位置满足即视为字段存在，null 是否有效仍由类型决定。
- `flat` 由 reflection/object parser 消费，将子对象字段展开到父 object；无命名或无法表达 flatten 的后端后续通过 Reader/Writer capability 或后端 Parser 特化处理。
- `raw_string` 由 string parser 消费；RapidJSON 通过 `Writer::RawValueType/parseRawValue` 和 `Reader::toRawString` 提供可选能力，其他后端可忽略该 JSON tag。
- `fixed_length` 是当前字段/值的宽度修饰，`unframed` 是当前 object 值在当前绑定位置上的布局修饰；
  仅声明相应 capability 的后端消费。
- JSON 收到 `fixed_length/unframed` 时保持普通具名 JSON 表达。

### Tag API 收敛目标

对序列化模块，保留的核心 API 尽量只覆盖“绑定”和“查询”两层：

- 绑定层：
  - `make_tags<Tags>(accessor)`
  - `TaggedField<Tags, Accessor>`
  - `field_accessor(...)`
  - `field_tags_v<T>`
- 查询层：
  - `tag_query::has_name(tags)` / `tag_query::name(tags)`
  - `tag_query::has_comment(tags)` / `tag_query::comment(tags)`
  - `tag_query::is_flat<T>(tags)`
  - `tag_query::is_skippable(tags)`
  - `tag_query::is_raw_string(tags)`
  - `tag_query::is_fixed_length(tags)` / `tag_query::fixed_length<T>(tags)`
  - `tag_query::is_unframed<T>(tags)`

查询层的统一约束：

- 查询 API 只表达“当前 tags 是否携带某语义”和“该语义的值是什么”。
- 查询 API 不暴露“消费当前 tag 后的剩余 tags”。
- 查询 API 不要求 tags 一定有 `base`，只要能被对应 query adapter 读取即可。
- 当前 struct-based tags 可以继续通过 `base` 兼容实现；未来 C++26 注解可通过 annotation adapter
  直接实现同一组 query，不必保留链式 wrapper 结构。

序列化路径中计划逐步停止使用的 API：

- `recursive_*`
- `has_recursive_*`
- `consume_*`
- `consume_recursive_*`
- `rebind_tag_base`

说明：

- 这些 API 已从 serialization tag 查询层删除，避免后续继续使用。
- serialization parser 不再依赖它们建立语义，也不再要求“多次 rename/comment 后重建剩余 tags”。

### 序列化中的 Tag 消费边界

目标是把“谁负责解释什么 tag”固定下来，避免同一个 tag 在多层重复消费，或者被错误地下传到子字段。

1. 通用 parser 入口

- 不负责 `name`、`flat`、`skippable`、`fixed_length`、`unframed` 的消费。
- 可保留对“附着在当前 parent 上的注释类信息”的统一处理，但实现上不再依赖 `consume_recursive_comment`。
- 更推荐 comment 和 field-name 一样，在实际拥有 parent/object-field 语义的地方消费。

2. reflection object-field parser

- 消费 `name`：决定 object field key。
- 消费 `skippable`：决定命名字段缺失时的行为。
- 消费 `flat`：决定是否把子 object 展开到父 object。
- 消费 `comment`：决定是否在该字段对应的父节点位置附加注释。
- 不把以上 tag 继续传给该 struct 的内部字段。

3. reflection positional parser

- 不消费 `name`、`skippable`、`flat`。
- 只按 positional 规则处理长度与索引错误。
- 字段各自只看自己的 field tags。

4. struct parser（当前 value 自身）

- 消费 `unframed`：只改变“当前 object 值在当前绑定位置上的布局”。
- 不把 `unframed` 继续传给成员字段。

5. basic/string/arithmetic parser

- string parser 消费 `raw_string`。
- arithmetic / binary-capable parser 消费 `fixed_length`。
- 它们只影响当前 value 的编解码，不影响外层 object/array 结构。

6. transparent wrapper parser

- `TaggedField`、`optional<T>`、`shared_ptr<T>`、`unique_ptr<T>`、`variant<Ts...>` 继续透传 tags。
- 原因：它们仍然代表“同一个逻辑槽位中的值”，不是新的字段层级。

7. structural container parser

- `tuple/pair`、`array/vector/list/deque/set/...`、`map/...` 不透传外层 tags 给元素。
- 原因：元素已经进入新的子槽位；如果元素需要 tags，应由它们自己的反射字段 metadata 或显式绑定提供。

### 设计约束

- 多次 `rename` 在 serializer 中没有明确语义；采用“最多一个生效值”的查询模型即可。
- 多次 `comment` 如需兼容旧写法，也只定义“当前查询返回一个 comment”，不要求支持按顺序层层消费。
- 不同 tag 的消费顺序不应构成公开语义。实现可以按 `if (flat) ...; if (has_name) ...; if (skippable) ...`
  组织，但外部不应依赖 tag 的链式顺序。
- 对 serializer 而言，tag 的问题规模不能通过“递归去掉一个 tag 后继续求解”自然缩小；更合适的模型是
  “针对当前 binding 做一组独立 query”。

### 代码迁移方向

1. 先引入统一查询层

- 在 `serialization/private/tags.hpp` 中新增或重命名一组 query 风格入口，名称尽量去掉 `recursive`/`consume`。
- 现有 struct-based tags 先在内部通过 `base` 实现这些 query，保证行为兼容。

2. 改写 serialization parser 使用方式

- `parsing/parser.hpp` 去掉对 `consume_recursive_comment` 的依赖。
- `parsing/reflection.hpp` 中对 object field 的写入/读取改为“同层 query + 同层消费”，不再 rebuild 剩余 tags。
- `sequence/map/tuple` 保持不透传外层 tags；`optional/pointer/variant` 明确保留透传并补注释说明语义。

3. 收紧旧 API 边界

- 删除 serialization 里的 `consume_*` / `rebind_tag_base`，避免新代码继续依赖。
- 测试从“链式拆解是否存在”改为“最终序列化语义是否正确”。

4. 为 C++26 注解预留适配点

- 注解层只需产出字段 metadata 或实现相同的 query adapter。
- serializer 不感知 tag 的底层表示是当前 struct、annotation adapter，还是未来别的 metadata provider。

已补测试：

- [x] `make_tags<JsonTag{.skippable = true}>(field)` 缺字段时成功并保留原值。
- [x] `make_tags<JsonTag{.raw_string = true}>(std::string{"{\"a\":1}"})` 写出 JSON object，而不是转义字符串。
- [x] `make_tags<JsonTag{.flat = true}>(nestedStruct)` 字段展开。
- [x] `make_tags<BinaryTag{.fixed_length = true}>(field)` 在 JSON 中不改变输出。
- [x] `BinaryTag{.unframed = true}` 从类型级 metadata 迁移为字段/调用级 metadata；JSON 收到该 tag 时仍输出具名对象。

## 分阶段迁移计划

### Phase 0: 行为锁定

目标：先让旧行为可验证，避免重构时把格式悄悄改掉。

工作：

- 为 primitive/string/enum 添加 roundtrip 测试。
- 为 vector/list/deque/array/set 添加 roundtrip 测试。
- 为 string-key map 和 non-string-key map 添加输出形状测试。
- 为 pair/tuple/variant 添加输出形状测试。
- 为 optional/pointer/null 添加缺失字段和 null 测试。
- 为 reflection struct 添加 named-object 和 positional-array 测试。
- 为 tags 添加最小行为测试。

此阶段不删除旧代码。

### Phase 1: 通用 Parser 核心

目标：建立独立于协议的 Parser 层。

工作：

- 将当前 `private/parser.hpp` 提升或迁移到 `serialization/parsing/parser.hpp`。
- 引入 Reader/Writer concepts。
- Parser write/read 直接模板化接收原始 `Tags`。
- tag 在实际消费层解释，未消费的后端专用 tag 可以被其他后端忽略。
- Parser 返回 `sa::Result<void>`，错误信息沿字段名和元素索引逐层追加上下文。
- 建立“透明包装透传、结构边界截断”的 tag 传播规则，并在 parser 实现中固定下来。

验收：

- `Parser` 头不包含 RapidJSON。
- RapidJSON serializer 能通过 `Parser<Reader, Writer, T>` 写入和读取 basic 类型。

### Phase 1.5: Tag Query 收敛

目标：让 serialization 不再依赖链式 tag 消费模型。

工作：

- 在 `serialization/private/tags.hpp` 中补齐 query 风格 API。
- 删除 `recursive_* / consume_* / rebind_tag_base`。
- `parser.hpp` 与 `parsing/reflection.hpp` 改为 query 风格实现，不再对 `name/comment` 做“消费后递归重入”。
- 明确 `optional/pointer/variant` 透传 tags，`struct/sequence/map/tuple` 不透传外层 tags。

执行 Checklist：

- [x] 保留 `NEKO_DEFINE_NESTED_TAG` 宏，作为后续批量新增 tag query 的生成工具。
- [x] 收窄 `NEKO_DEFINE_NESTED_TAG`，只生成 `tag_query::xxx(tags)` 和 `tag_query::has_xxx(tags)`。
- [x] 删除 serialization tag 查询层的 `tag_access` 命名空间。
- [x] 删除 serialization tag 查询层的 `recursive_*` / `has_recursive_*` / `consume_*` / `consume_recursive_*`。
- [x] 删除 `tag_detail::comment_tag_impl` / `tag_detail::rename_tag_impl` 的 `rebind_base` 以及 `detail::rebind_tag_base`。
- [x] `basic.hpp` 使用 `tag_query` 读取 `raw_string` / `fixed_length`。
- [x] `parser.hpp` 移除通用入口里的 comment 递归消费逻辑。
- [x] `reflection.hpp` 的 named object field 写入使用 `tag_query` 查询 `name/comment/skippable/flat`。
- [x] `reflection.hpp` 的 named object field 读取不再 rebuild 剩余 tags。
- [x] `reflection.hpp` 的 positional field comment 写入移动到 positional field 槽位。
- [x] `test_tags.cpp` 从验证链式拆解改为验证最终 query 语义。
- [x] `test_xml.cpp` 改为验证 `tag_query` comment/name 查询以及 XML comment 输出。
- [x] 运行 `xmake run test_tags`。
- [x] 运行 `xmake run test_xml`。
- [x] 运行 `xmake run test_json_backend`。
- [x] 运行 `xmake run test_binary_serializer`。
- [x] argparser 模块同步收敛到 query 风格 API，删除自己的 `tag_access` 命名空间。
- [x] argparser tag 查询改为复用 `NEKO_DEFINE_NESTED_TAG`，避免每个 tag 手写重复 query 函数。
- [x] 删除 argparser tag 中无用的 `rebind_base` 符号。
- [x] 确认 serialization/argparser 内不再保留 `tag_access` / `recursive_*` / `consume_*` / `rebind_base`。
- [x] 运行 `xmake build test_argparser`。
- [x] 新增公共反射 tag 头 `global/reflection_tags.hpp`，承载 `TaggedField` / `make_tags` / `NoTags` / `field_tags_v`。
- [x] 将 `NEKO_DEFINE_NESTED_TAG` 从 serialization private 头移到 `global/reflection_tags.hpp`，供 serialization / argparser / rpc 共用。
- [x] `serialization/private/tags.hpp` 收敛为 serialization 具体 tag 与 query 语义，不再定义反射基础设施。
- [x] `rpc/tags.hpp` 改为依赖公共反射 tag 头，并删除残留 `rebind_base`。

验收：

- serialization 目录内不再需要 `consume_recursive_*`。
- serialization 目录内不再需要 `rebind_tag_base`。
- 现有 JSON/binary tag 行为测试继续通过。

### Follow-up: ProtoBase 与 ilias 隔离检查

背景：`xmake build test_json_serializer` 在当前 static 配置下链接到了旧的 `libNekoProtoBase.so`，该共享库带有 `libilias.so.0` 依赖，导致 proto 单测被 communication/runtime 依赖穿透。

执行 Checklist：

- [x] 确认当前配置为 `kind=static`、`enable_communication=false`、`enable_jsonrpc=false`。
- [x] 确认 `include/nekoproto/proto/proto_base.hpp` 与 `src/proto_base.cpp` 不直接引用 ilias。
- [x] 确认当前 `NekoProtoBase` 目标只编译 `src/proto_base.cpp`，不包含 `src/communication_base.cpp`。
- [x] 确认 `libNekoProtoBase.a` 内只有 `proto_base.cpp.o`，且没有 ilias 未定义符号。
- [x] 确认 build 目录残留旧 `libNekoProtoBase.so`，其 dynamic section 里有 `NEEDED libilias.so.0`。
- [x] 确认 `test_json_serializer` 链接命令使用 `-Lbuild/linux/x86_64/debug -lNekoProtoBase`，因此会优先命中同目录旧 `.so`。
- [x] 扩展 `target_autoclean`，按当前 static/shared kind 清掉同名冲突产物，避免旧 `.so` / `.a` 截胡。
- [x] 将 communication 代码从 `NekoProtoBase` 目标拆出为独立 `NekoCommunication` 目标。
- [x] 收窄 `auto_add_packages` 对 ilias 的注入范围，只给显式 `uses_ilias` 的目标添加。
- [x] communication 单测改依赖新的 `NekoCommunication` 目标，而不是依赖 `NekoProtoBase` 承载 transport 能力。

### Phase 2: RapidJSON 变成薄后端

目标：RapidJSON 只提供 Reader/Writer 和 native value specialization。

工作：

- `rapid_json_reader.hpp` 对齐 Reader concept 命名。
- `rapid_json_writer.hpp` 对齐 Writer concept 命名。
- `rapid_json_serializer.hpp` 的 `operator()` 只做入口：
  构造 root writer/reader，调用通用 Parser，flush。
- `RapidJsonValue` 的读写特化保留在 JSON 后端，因为它是 backend-native 类型。
- 删除 `rapid_json_parser.hpp`，RapidJSON 只保留 Reader/Writer 和 native value Parser 特化。

验收：

- `JsonSerializer::OutputSerializer/InputSerializer` 外部调用不变。
- 新 RapidJSON 路径不继承旧 `detail::OutputSerializer/InputSerializer`。
- 新 RapidJSON 路径不调用旧 `save/load` 分发。

### Phase 3: `types/xxx.hpp` 迁移为通用 Parser

目标：把当前容器适配层从 `save/load` 改成通用 Parser。

建议顺序：

1. `private/helpers.hpp` 中 basic save/load 对应的 primitive/string/null parser。
2. `types/enum.hpp`、`types/byte.hpp`、`types/bitset.hpp`、`types/atomic.hpp`。
3. `types/optional.hpp`、`types/shared_ptr.hpp`、`types/unique_ptr.hpp`。
4. `types/vector.hpp`、`types/list.hpp`、`types/deque.hpp`、`types/array.hpp`。
5. `types/set.hpp`、`types/multiset.hpp`、`types/unordered_set.hpp`、`types/unordered_multiset.hpp`。
6. `types/pair.hpp`、`types/tuple.hpp`。
7. `types/map.hpp`、`types/multimap.hpp`、`types/unordered_map.hpp`、`types/unordered_multimap.hpp`。
8. `types/variant.hpp`。
9. `types/binary_data.hpp`、`types/u8string.hpp`。
10. `types/struct_unwrap.hpp` 和 `reflection.hpp`。

迁移规则：

- 每迁一个类型，先添加 `Parser<Reader, Writer, T>`。
- 对新 RapidJSON 路径，优先走 Parser。
- 旧 `save/load` 暂时保留，作为旧 binary/xml/simdjson 和行为对照。
- 每迁一组类型，就增加或恢复对应测试。

### Phase 4: 兼容桥收缩

目标：旧 `save/load` 只作为临时兼容桥，不再承担主路径。

工作：

- 对已经迁移的类型，旧 `save/load` 可以反过来调用 Parser，或保持旧实现直到所有旧后端迁移完成。
- 对自定义类型的 `value.save/load`，提供明确迁移路径：
  推荐写 `Parser<Reader, Writer, T>` 特化，或通过反射元信息序列化。
- 将 `traits::has_function_save/load` 从新 serializer 分发中移除。
- 保留 public `OutputSerializer/InputSerializer` 类型名，避免用户调用点大面积改名。

验收：

- 新 JSON 测试不依赖 `startObject/startArray/startNode`。
- 旧测试中直接操作 serializer 状态机的用例被改写为 `operator()` 或移到 legacy 测试。

### Phase 5: 非 JSON 后端迁移

目标：binary/xml/simdjson 后端也接到同一 Parser 层。

当前状态：

- [x] simdjson：Reader 包装 DOM handle 与 parser 生命周期；Writer 复用后端无关 JSON 文本树。
- [x] binary：使用顺序 cursor/frame Reader 和 stream-style Writer，不伪装成 JSON DOM。
- [x] binary：`fixed_length` 通过 Reader/Writer 可选能力消费，JSON 后端忽略该修饰。
- [x] schema：Parser 生成后端无关类型 schema，再转换为 JSON Schema Draft-07。
- [x] xml：pugixml Reader/Writer 接入通用 Parser；支持元素、属性读取、`xml_content` 和数组 roundtrip。

重点：

- binary 的 fixed-length 应走 `fixed_length` tag 和 schema/format capability。
- XML 不应强行复用 JSON raw_string。
- schemaful 格式要区分 object、map、union。

### Phase 6: 删除旧实现

满足以下条件后再删：

- RapidJSON、binary、xml、simdjson 的目标后端策略明确。
- 所有 `types/xxx.hpp` 的主逻辑已迁到 Parser。
- 测试不再直接依赖旧 serializer 状态机。
- 自定义类型迁移文档完成。

可删除或大幅收缩：

- `detail::OutputSerializer<SelfT>`。
- `detail::InputSerializer<SelfT>`。
- `traits::has_function_save/load` 在新路径中的使用。
- `traits::has_method_save/load` 在新路径中的使用。
- `TestSerializableTrait`。
- 各类型文件中只为旧状态机服务的 `save/load`。
- RapidJSON 里通用类型规则版本的 `RapidJsonParser<T>`（JSON 路径已删除）。
- `startObject/startArray/startNode/finishNode/name/rollbackItem` 作为 public serializer API。

不应删除：

- `NameValuePair` 或其替代字段绑定类型，除非已有新 `Field<T>` 完整替代。
- `tags.hpp` 的 tag 定义，应该迁移/整理而不是删除。

## 文件级迁移表

| 文件 | 当前内容 | 新接口/新位置 | 删除时机 |
| --- | --- | --- | --- |
| `private/helpers.hpp` | `NameValuePair`、旧 CRTP serializer、basic `save/load` | 字段 helper 保留或迁到 `parsing/field.hpp`；basic parser 迁到 `parsing/basic.hpp` | 全部后端迁移后删除旧 CRTP 和 basic `save/load` |
| `private/traits.hpp` | save/load 探测、optional-like、minimal traits | traits 拆到 `parsing/traits.hpp` | 新 serializer 不再用 save/load 探测后收缩 |
| `private/tags.hpp` | tag 定义与 `tag_query` | 保留；原始 tag 直接传入 Parser | 不删除，只整理 |
| `types/atomic.hpp` | 已有 Parser 雏形 + 旧 save/load | 通用 `Parser<Reader,Writer,std::atomic<T>>` | 后端全迁后删旧 save/load |
| `types/optional.hpp` | 已有 Parser 雏形 + 旧 save/load | 通用 `Parser<Reader,Writer,std::optional<T>>` | 后端全迁后删旧 save/load |
| `types/vector/list/deque/array.hpp` | 旧数组 save/load | `parsing/sequence.hpp` 或文件内通用 Parser | 对应测试通过后删旧实现 |
| `types/set/multiset/unordered_set/unordered_multiset.hpp` | 旧数组 save/load | 复用 sequence parser，读时 insert | 对应测试通过后删旧实现 |
| `types/map/multimap/unordered_map/unordered_multimap.hpp` | string key object，非 string key array of `{key,value}` | `parsing/map.hpp` | map 行为测试通过后删旧实现 |
| `types/pair.hpp` | `{first,second}` | `parsing/tuple.hpp` 或文件内 Parser | pair/tuple 测试通过后删旧实现 |
| `types/tuple.hpp` | positional array | `parsing/tuple.hpp` | tuple 测试通过后删旧实现 |
| `types/variant.hpp` | variant/monostate 旧逻辑 | `parsing/variant.hpp` | variant 形状测试锁定后迁 |
| `types/struct_unwrap.hpp` | reflection save/load | `parsing/reflection.hpp` | reflection + tags 测试通过后删旧实现 |
| `types/binary_data.hpp` | BinaryData + RapidJSON 特化 | 通用 BinaryData parser，backend-specific base64/raw 策略进入 capability | BinaryData 测试通过后删旧实现 |
| `json/rapid_json_parser.hpp` | 旧 RapidJSON parser 过渡层 | 已删除；通用 include 使用 `parsing/parsers.hpp` | 已完成 |
| `json/rapid_json_reader.hpp` | RapidJSON Reader | 保留，改成 concept-compatible Reader | 不删除 |
| `json/rapid_json_writer.hpp` | RapidJSON Writer | 保留，改成 concept-compatible Writer | 不删除 |
| `json/rapid_json_serializer.hpp` | public serializer + RapidJSON value | 保留 public API，内部只调 Parser | 不删除 |
| `json/simd_json_serializer.hpp` | 旧 simdjson 后端 | 后续改 read-only Reader 或完整 backend | simdjson 策略确定后处理 |
| `binary_serializer.hpp` | 旧 binary 状态机 | 后续实现 binary Reader/Writer | binary 新后端完成后删旧状态机 |
| `xml_serializer.hpp` | 旧 XML 状态机 | 后续实现 XML Reader/Writer | XML 新后端完成后删旧状态机 |

## 测试迁移计划

需要优先调整的测试：

- `tests/unit/proto/test_json_serializer.cpp`
  已整体改成 `operator()` roundtrip、JSON 形状和 stream/schema 断言，不再调用
  `startObject/startNode/finishNode` 等已移除状态机 API。

- `tests/unit/proto/test_serializer_in_main.cpp`
  保持作为真实复杂对象 roundtrip。

- `tests/unit/proto/test_random_proto.cpp`
  保持随机结构压力测试。

- `tests/unit/common/test_traits.cpp`
  旧 serializer trait 测试需要拆成 legacy traits 与 parser concepts 两类。

新增建议：

- `test_parser_basic.cpp`
- `test_parser_sequence.cpp`
- `test_parser_map.cpp`
- `test_parser_optional_variant.cpp`
- `test_parser_reflection_tags.cpp`
- `test_rapid_json_backend.cpp`

当前验证状态：

- `test_json_backend`：28/28，覆盖通用 Parser 的 basic、容器、反射、tag 和错误传播路径。
- `test_simd_json_backend`：8/8，覆盖 simdjson Reader、共享 TextWriter、raw JSON 与 native value 生命周期。
- `test_binary_serializer`：11/11，覆盖旧 wire layout、fixed-length/type-layout tag、嵌套具名字段、schema 元数据、
  宽度不匹配错误和截断输入错误。
- `test_json_serializer`：RapidJSON 与 simdjson 配置均为 15/15，覆盖主要类型族、复杂对象、
  iostream 和通用 Parser 生成的 JSON Schema。
- `test_xml`：覆盖 pugixml Reader/Writer、对象字段、数组同名兄弟元素、`xml_content`、rename/comment tags。
- `test_to_string`：覆盖通用 Parser 的 human-readable writer，并验证字段名可与宏内部 metadata 名称共存。
- `NekoJsonRpc` 已切换到 `ParserResult` 并可独立构建。

## 每阶段验收标准

每个阶段至少满足：

- 新增或迁移的类型能通过 RapidJSON roundtrip。
- 输出形状与旧行为基线一致。
- public 调用仍是 `serializer(value)`。
- Parser 失败必须保留具体 `sa::ErrorCode` 和可定位 message。
- 新代码路径不调用旧状态机 API。
- 新 Parser 头不包含具体协议库头。
- 后端专用 tag 的忽略/失败规则有测试或文档约束。

最终完成标准：

- `types/xxx.hpp` 主逻辑全部是通用 Parser。
- RapidJSON parser 文件不再承载通用容器/反射逻辑。
- `save/load` 只作为 legacy 兼容或彻底删除。
- 所有后端都通过同一 Parser 层适配 C++ 类型。
- tags 在 reflection 字段和 wrapper 类型上正式生效。

## 风险与处理

- 风险：一次性删除旧 `save/load` 会导致 binary/xml/simdjson 断裂。
  处理：旧实现保留到对应后端迁移完成。

- 风险：当前 RapidJSON parser 已经能跑，继续堆功能会形成新的 JSON-only 架构。
  处理：只允许 RapidJSON 文件保留 native value 和后端 Reader/Writer；容器/反射规则必须逐步移出。

- 风险：`raw_string` 天然偏 JSON，放到通用 tag 后可能误导非 JSON 后端。
  处理：string Parser 只在 Reader/Writer 声明 raw-value 能力时启用；其他后端按普通字符串处理。

- 风险：旧 size helper 模型和 schemaful 格式冲突。
  处理：新 Reader 直接暴露 array/object/map 大小，不把额外 size tag 作为协议能力。

- 风险：旧自定义类型只写了 `save/load`。
  处理：迁移期保留 legacy bridge；长期推荐反射元信息或 Parser 特化。
