# NekoProtoTools 源码审查与修复计划

> 审查对象：`NekoProtoTools`  
> 项目版本：`0.3.2`（来自 `xmake.lua`）  
> ZIP SHA-256：`c5dddb2ac1ed61838ba9aef5f7015cd5b557efe3ec14a060dd38fa337160d068`  
> 审查日期：2026-07-13  
> 审查范围：序列化框架、自定义二进制格式、Neko RPC、JSON-RPC、传输端点、测试和 CI  
> 代码规模：约 146 个 C/C++ 源文件、约 3.7 万行代码

## 文档定位与阅读顺序

本文同时承担“历史问题记录”和“后续设计规范”两种职责。为避免把已经修复的问题误读成当前状态，统一按以下顺序阅读：

1. 第 3～6 节保留 Binary V1 和原 RPC 实现的审查证据，描述的是修复前基线；
2. 第 7 节是当前实现及下一步目标态的设计规范，新增 wire 字节解读、tag 传播、`IdObject` 和 union/variant 规则；
3. 第 8 节是唯一的实施状态来源；只有标记为“已完成”的项目才能视为已经落地；
4. “建议”“目标态”“待实施”均不是当前行为，不能据此生成兼容数据。

## 1. 审查方法与结论说明

本报告以 ZIP 中的源码为准，不依赖 GitHub 网页内容。审查方式包括：

1. 对序列化 Reader/Writer、通用 parser、RPC dispatcher、client/server、frame codec、method-id、压缩和传输端点进行静态代码审查；
2. 使用 Clang 17 编译若干独立最小复现程序，验证二进制序列化关键问题；
3. 检查现有单元测试、fuzz target 和 GitHub Actions 工作流；
4. 按“已复现”“静态确认”“高风险待运行验证”区分证据强度。

原始审查阶段没有可用的 xmake/完整第三方构建依赖，因此第 3～6 节中标记为“已复现”的历史问题来自独立最小程序；依赖 ILIAS 调度语义的原始判断保留为“高风险待运行验证”。修复阶段已经具备本地 xmake 环境，实际回归结果单独记录在第 8 节，不能用后来的测试倒改原始证据等级。

### 1.1 优先级定义

| 优先级 | 含义 |
|---|---|
| **P0** | 会造成确定的数据错误、协议不可逆、跨连接串扰或明显的安全风险，应立即修复 |
| **P1** | 会造成重要功能错误、兼容性问题、拒绝服务风险或严重性能限制，应在近期版本修复 |
| **P2** | 健壮性、错误语义、可维护性或边界行为问题，应纳入常规修复计划 |
| **P3** | API 一致性、性能优化、工程质量或长期演进建议 |

### 1.2 证据等级

| 标记 | 含义 |
|---|---|
| **已复现** | 已编译并运行最小程序，结果稳定出现 |
| **静态确认** | 从控制流和数据结构可直接确认，不依赖运行时调度假设 |
| **高风险待验证** | 代码结构存在明显风险，但最终表现依赖 ILIAS 调度、取消或线程模型，应增加集成测试/TSan 验证 |

---

## 2. 执行摘要

原始审查最需要优先处理的是两组问题。当前完成情况以第 8 节为准。

### 2.1 二进制序列化格式存在确定的不可逆问题

Binary V1 的以下场景已确认不能正确 round-trip：

- `std::optional<std::string>{"null"}` 被读成 `std::nullopt`；
- `std::variant<int, std::string>{"abc"}` 被读成整数 `3`，且留下未消费字节；
- 反射对象包含空 optional 时，Writer 写出的对象成员数与实际成员数不一致；
- 使用 `flat` 展开的嵌套对象会破坏父对象成员计数；
- 未知枚举值按整数写入后，Reader 的“先尝试字符串、再尝试整数”逻辑无法回退；
- 浮点数使用宿主机字节序，跨大小端平台不兼容。

这些问题说明当前 standalone binary format 缺少：

- 明确的值类型标记；
- variant discriminator；
- 可回滚的 Reader checkpoint；
- 正确的动态对象成员计数；
- magic/version；
- 可跳过字段的 wire type 和 length。

### 2.2 RPC 状态没有按连接和请求批次隔离

`RpcServer` 的多个 endpoint 共享同一个 dispatcher，而 dispatcher 又共享：

- 当前请求 ID 列表；
- 取消 handle 表；
- TaskGroup；
- 方法任务的响应容器写入路径。

不同客户端使用相同 request ID 时，会覆盖取消 handle。不同连接的批次还可能互相等待或清理状态。除此之外，RPC 还存在：

- frame 长度无策略上限；
- 解压结果无输出预算；
- 协议声明了 `Cancel` 帧，但服务端实际不会执行取消；
- 客户端所有调用被一把 mutex 完全串行化；
- `close()` 后仍报告已连接；
- JSON-RPC 对 parse error、空 batch 和非法 batch item 的处理不符合预期；
- 远端 method table 可使用稀疏超大 ID 触发巨型 vector resize。

### 2.3 建议版本策略

二进制 wire format 的正确修复很可能无法保持字节级兼容。建议：

1. 将当前格式明确命名为 Binary V1；
2. 新增带 magic、version、type/wire-type、长度和 variant index 的 Binary V2；
3. 在一个过渡版本中保留 V1 只读或显式 opt-in；
4. RPC frame 已有协议版本，但 payload serializer 的版本也应进入握手或 codec ID；
5. 为 V1/V2 建立固定 golden vectors，避免未来再次无意破坏兼容性。

---

# 3. 序列化问题与修复列表

<a id="ser-001"></a>
## SER-001：二进制 `null` 与普通字符串 `"null"` 编码碰撞

- **优先级：P0**
- **证据：已复现**
- **位置：**
  - `include/nekoproto/serialization/binary/binary_writer.hpp:37-39,108-116`
  - `include/nekoproto/serialization/binary/binary_reader.hpp:108-119`
  - `include/nekoproto/serialization/parsing/optional.hpp:24-39`

### 原因

Binary Writer 使用普通字符串编码表示 null：

```cpp
_writeString("null");
```

Binary Reader 的 `isEmpty()` 则预读一个字符串，并把内容等于 `"null"` 的值当成空值。这使协议数据和控制标记共用同一编码空间。

### 已确认表现

输入：

```cpp
std::optional<std::string> value = std::string("null");
```

最小复现结果：

```text
optional_ok=1 has=0
```

序列化操作成功，但反序列化结果变成 `std::nullopt`。

### 影响

- 合法字符串 `"null"` 无法可靠保存；
- `optional<string>`、智能指针和包含 `monostate` 的类型可能发生数据丢失；
- RPC 参数或返回值使用 BinarySerializer 时可能被静默篡改；
- 该碰撞无法通过上层 schema 修复，因为 wire bytes 本身完全相同。

### 修复方案

**推荐方案：Binary V2 使用显式 type tag。**

```text
[type-tag][payload]
0x00 = null
0x01 = bool
0x02 = signed integer
0x03 = unsigned integer
0x04 = float
0x05 = string
0x06 = array
0x07 = object
```

短期不建议换成其他特殊字符串，因为任何字符串 sentinel 都会继续与合法业务数据冲突。

### 兼容性说明

这是 wire-format breaking change。应通过新协议版本或新 codec ID 引入，不应在不改版本的情况下直接改变已有字节含义。

### 验收标准

- `null` 与字符串 `"null"` 编码不同；
- `optional<string>{"null"}` 可稳定 round-trip；
- 空 optional、空智能指针和 `monostate` 都有唯一编码；
- golden vectors 中固定 null 和字符串的字节序列。

### 建议测试

```cpp
TEST(BinaryV2, NullDoesNotCollideWithStringNull);
TEST(BinaryV2, OptionalStringNullRoundTrips);
TEST(BinaryV2, NullSmartPointersRoundTrip);
```

---

<a id="ser-002"></a>
## SER-002：二进制 `std::variant` 没有 discriminator，且失败尝试会污染流位置

- **优先级：P0**
- **证据：已复现**
- **位置：**
  - `include/nekoproto/serialization/parsing/variant.hpp:42-87`
  - `include/nekoproto/serialization/binary/binary_reader.hpp:22-56,122-149`

### 原因

variant Writer 只写当前 alternative 的值，没有写 active index 或类型名称。Reader 按模板参数顺序逐个尝试：

```cpp
parser_read<R>(in, tmp, tags);
```

Binary Reader 的所有候选共享同一个 `State::offset`，且 `InputValue::frame` 会被标记为 consumed。候选解析失败后没有恢复 offset 和 frame 状态。

即使增加回滚，仅靠“尝试第一个能解析的类型”仍然无法消除 `int/string`、多个对象类型或多个容器类型之间的歧义。

### 已确认表现

输入：

```cpp
std::variant<int, std::string> value = std::string("abc");
```

字符串的长度前缀首先被成功解释为整数：

```text
variant_ok=1 index=0 int=3
reader_offset=1 total_size=4
```

结果错误地选择了 `int`，并留下 `abc` 三个未消费字节。

### 影响

- variant 数据不可逆；
- 后续字段会从错误 offset 继续解析；
- alternative 顺序改变会改变协议含义；
- RPC 参数/返回值包含 variant 时可能造成跨字段污染；
- JSON 等自描述格式也可能在多个候选均可接受时产生顺序依赖。

### 修复方案

统一的目标逻辑形态为：

```text
[alternative-index, payload]
```

Binary V2 已使用 `ValueTag::Array + count(2) + UnsignedInteger(index) + payload` 实现该形态，完整字节示例见 7.1.6。其他 backend 的统一策略和显式 untagged 兼容模式见 7.4。

默认 tagged 路径不需要试读 alternative。Reader checkpoint 只保留给显式 `UnionEncoding::Untagged`、格式探测等兼容场景：

```cpp
auto checkpoint = reader.checkpoint();
reader.restore(checkpoint);
```

### 验收标准

- 所有 alternative 都保留原 active index；
- 任意失败解析不改变主 Reader 状态；
- variant 后方的字段仍从正确 offset 读取；
- alternative 顺序被明确视为 wire ABI；重排或中间插入必须升级协议，未来可用稳定 alternative ID 消除该限制。

### 建议测试

```cpp
TEST(BinaryV2, VariantWritesActiveIndex);
TEST(BinaryV2, VariantStringIsNotDecodedAsLengthInteger);
TEST(BinaryV2, VariantRejectsUnknownIndex);
TEST(Variant, UntaggedFailedAlternativeRestoresReaderState);
```

---

<a id="ser-003"></a>
## SER-003：空 optional 被跳过，但二进制对象成员数量仍按静态字段数写入

- **优先级：P0**
- **证据：已复现**
- **位置：**
  - `include/nekoproto/serialization/parsing/reflection.hpp:21-33`
  - `include/nekoproto/serialization/parsing/reflection.hpp:60-72`
  - `include/nekoproto/serialization/parsing/reflection.hpp:154-193`
  - `include/nekoproto/serialization/parsing/reflection.hpp:282-283`
  - `include/nekoproto/serialization/binary/binary_writer.hpp:32-34`

### 原因

`parser_reflect_field_count<T>()` 在编译期统计全部未 ignore 的字段。写对象时先把这个固定数量写入 binary stream：

```cpp
auto object = addObject(writer, parser_reflect_field_count<T>(), ...);
```

但实际写字段时，空 optional-like 字段默认被 `parser_should_skip_empty_field()` 跳过。因此“声明的对象字段数”大于“实际输出的字段数”。

### 已确认表现

结构体：

```cpp
struct Value {
    std::optional<int> a;
    int b = 42;
};
```

当 `a == nullopt` 时，Writer 声明有两个字段，实际只输出 `b`。Reader 读取 `b` 后仍认为对象还有一个字段，最终返回：

```text
Required field 'b' is missing
```

### 影响

- 任何含空 optional 的命名反射对象都可能无法 round-trip；
- `shared_ptr`、`unique_ptr` 等 optional-like 类型也可能受影响；
- 嵌套对象会进一步污染父容器的 offset；
- 这是默认配置即可触发的常见数据模型问题。

### 修复方案

可选方案：

1. **推荐：先计算实际 emitted field count，再创建对象 frame。**
2. Binary V2 使用对象 payload length 或结束标记，不依赖静态字段数。
3. 对 V1 临时开启 `NEKO_WRITE_NULL_FOR_EMPTY_OPTIONAL`，使声明数量和实际数量一致，但这只能缓解 optional，不能修复 flat 展开和协议演进问题。

建议抽取统一函数：

```cpp
runtime_reflect_field_count(value)
```

它必须考虑：

- ignore；
- empty optional；
- flat 字段展开后的实际成员数量；
- backend 的 null/omit 策略。

### 验收标准

- 空 optional 位于对象首部、中部、末尾时均可 round-trip；
- 多个空 optional 不会改变后续字段读取；
- Writer 声明数量等于实际写出的成员数量；
- JSON/XML 等不依赖成员数量的 backend 行为保持不变。

---

<a id="ser-004"></a>
## SER-004：`flat` 字段展开多个成员，但父对象只把它计为一个字段

- **优先级：P0**
- **证据：已复现**
- **位置：**
  - `include/nekoproto/serialization/parsing/reflection.hpp:60-72`
  - `include/nekoproto/serialization/parsing/reflection.hpp:161-165`
  - `include/nekoproto/serialization/parsing/reflection.hpp:202-206`
  - `include/nekoproto/serialization/parsing/reflection.hpp:282-283`

### 原因

`flat` 字段在写入时递归把嵌套对象的所有字段写进父对象，但静态计数只把该字段计为 `1`。例如：

```text
Outer fields: [inner(flat), z] => declared count = 2
Actual emitted fields: [inner.x, inner.y, z] => actual count = 3
```

Reader 在读取两个成员后会认为父对象结束，后续字段丢失或报错。

### 已确认表现

包含两个字段的 flat 子对象加一个父对象字段时，子对象字段可以读出，但父字段 `z` 报缺失。

### 影响

- Binary backend 的 `flat` tag 对多字段嵌套对象不可靠；
- flat 与 optional、rename、skippable 组合时错误更难定位；
- schema 显示的是扁平字段，但 wire frame count 与 schema 不一致。

### 修复方案

- 动态计算 flat 展开后的实际成员数量；
- 或在 Binary V2 中让对象使用 byte length/TLV，不再依赖成员数完成跳过；
- 对 flat 嵌套递归设置最大深度，避免无限/过深展开。

### 验收标准

- 一层、多层 flat 均可 round-trip；
- flat 子对象包含 optional/ignored 字段时计数正确；
- flat 字段前后均有普通字段时读取顺序正确。

---

<a id="ser-005"></a>
## SER-005：枚举“先字符串后整数”的回退策略无法在 Binary Reader 上工作

- **优先级：P1**
- **证据：已复现**
- **位置：**
  - `include/nekoproto/serialization/parsing/basic.hpp:284-315`
  - `include/nekoproto/serialization/binary/binary_reader.hpp:122-149`

### 原因

未知枚举值会按 underlying integer 写入。读取时却先尝试字符串：

```cpp
if (parser_read_string<R>(in, enumName, tags)) { ... }
parser_read<R>(in, raw, tags);
```

第一次尝试会消费 Binary Reader 的 frame/offset；第二次尝试收到 `Binary value was already consumed`。

### 已确认表现

把枚举 raw value `7` 写入后再读取，返回：

```text
Failed to parse enum value: Binary value was already consumed
```

### 影响

- 未在反射表中登记的枚举值无法 round-trip；
- 服务器和客户端枚举版本不一致时兼容性差；
- 任何“先探测 A，再回退 B”的通用 parser 都可能复现同类问题。

### 修复方案

- Binary V2 用 type tag 区分枚举名称和整数；
- Reader 增加 checkpoint/rollback；
- 更稳妥的枚举策略是固定 wire representation，例如始终写 underlying integer，名称只用于文本格式；
- 若保留名称模式，应在协议配置中明确 `EnumEncoding::Name/Integer/Tagged`。

### 验收标准

- 已知和未知枚举值均可按配置 round-trip；
- 枚举读取失败不会消费后续字段；
- 新增枚举值可被旧端按 underlying value 保存或明确拒绝。

---

<a id="ser-006"></a>
## SER-006：浮点数直接复制宿主机内存，字节序和表示不稳定

- **优先级：P1**
- **证据：已复现**
- **位置：**
  - `include/nekoproto/serialization/binary/binary_writer.hpp:159-170`
  - `include/nekoproto/serialization/binary/binary_reader.hpp:138-145`

### 原因

浮点值使用 `_appendBytes(&value, sizeof(U))` 和 `memcpy`，没有像 fixed integer 一样做 endian 转换。

当前小端环境中，`double(1.0)` 的实际输出为：

```text
00 00 00 00 00 00 f0 3f
```

这是小端 IEEE-754 表示。

### 影响

- 小端与大端机器之间不兼容；
- 持久化文件依赖生成机器架构；
- RPC 在异构平台上可能得到错误数值；
- NaN payload、负零等细节没有规范化规则。

### 修复方案

- 要求 `std::numeric_limits<T>::is_iec559`；
- 使用 `std::bit_cast<uint32_t/uint64_t>`；
- 统一转换成协议指定字节序，例如 network byte order；
- 文档明确 NaN、infinity、negative zero 的保留或规范化策略。

### 验收标准

- 不同平台输出相同 golden bytes；
- float/double 的 normal、subnormal、±0、±inf、NaN 均有测试；
- 不支持的浮点表示在编译期或运行时明确拒绝。

---

<a id="ser-007"></a>
## SER-007：二进制对象严格依赖字段顺序，且无法跳过未知字段

- **优先级：P1**
- **证据：静态确认**
- **位置：**
  - `include/nekoproto/serialization/binary/binary_writer.hpp:59-73,86-116`
  - `include/nekoproto/serialization/binary/binary_reader.hpp:72-105`

### 原因

对象成员编码为：

```text
[field-name string][value bytes]
```

Reader 读取“下一个字段名”，并要求它与当前反射字段完全相等。字段值没有 wire type 和 payload length，因此当名称不匹配时，Reader 不知道该跳过多少字节。

此外，`objectField()` 在确认名称匹配前已经推进 offset 和成员 index，字段查找本身是破坏性的。

### 影响

- 字段重排会破坏兼容性；
- 新版本插入字段后，旧版本无法安全跳过；
- 删除字段或条件字段容易造成 offset 错位；
- `skippable` 对 Binary backend 的实际能力有限；
- 无法实现 protobuf 类似的向前/向后兼容。

### 修复方案

Binary V2 对对象字段采用 TLV 或 key/wire-type：

```text
[field-id][wire-type][payload-length][payload]
```

建议使用稳定数字 field ID，而不是仅使用字段名。字段名可作为调试信息或 schema 元数据，不必每次上 wire。

### 验收标准

- Reader 可以跳过未知字段；
- 字段顺序变化不影响解析；
- 新增 optional 字段不破坏旧客户端；
- 重复 field ID 有明确策略；
- schema 中保存 field ID，构建时检测冲突。

---

<a id="ser-008"></a>
## SER-008：字符串、容器、嵌套深度和总分配量没有解析预算

- **优先级：P1（安全）**
- **证据：静态确认**
- **位置：**
  - `include/nekoproto/serialization/binary/binary_reader.hpp:203-220,257-267`
  - `include/nekoproto/serialization/parsing/sequence.hpp:61-77`
  - `include/nekoproto/serialization/parsing/map.hpp:61-134`

### 原因

容器数量和字符串长度直接来自输入。Reader 只检查字符串是否超过剩余输入，但没有：

- 最大字符串字节数；
- 最大容器元素数；
- 最大对象字段数；
- 最大嵌套深度；
- 单次解析最大总分配量；
- 最大处理时间或元素预算。

### 影响

- 不可信输入可触发巨型循环或内存分配；
- RPC 暴露在网络上时可形成拒绝服务；
- 深度嵌套可造成栈耗尽；
- 即使输入字节较小，嵌套容器也可能产生大量对象构造开销。

### 修复方案

为所有 Reader 引入统一限制：

```cpp
struct ParseLimits {
    size_t max_input_bytes;
    size_t max_string_bytes;
    size_t max_container_elements;
    size_t max_object_fields;
    size_t max_depth;
    size_t max_total_allocated_bytes;
};
```

Reader state 应持有剩余预算，并在进入容器、构造字符串和插入元素前扣减。

### 验收标准

- 超限输入在分配前返回明确错误；
- 限制可由 RPC backend options 配置；
- 默认值是安全且有限的，不是 `SIZE_MAX`；
- fuzz 测试中内存和执行时间保持在预算内。

---

<a id="ser-009"></a>
## SER-009：反序列化成功时不检查输入是否被完整消费

- **优先级：P1**
- **证据：已复现**
- **位置：**
  - `include/nekoproto/serialization/serializer_adapter.hpp:92-106`
  - `include/nekoproto/serialization/binary_serializer.hpp:43-51`
  - `include/nekoproto/rpc/backend.hpp:799-803`

### 原因

`InputSerializerAdapter::operator()` 只检查目标值是否解析成功，不调用 finish，也不比较 `offset == size`。Neko RPC 的 `_decodePayload()` 同样只返回 `in(value)`。

### 已确认表现

对合法整数编码末尾追加一个任意字节，反序列化仍返回成功：

```text
ok=1 value=7 offset=1 total_size=2
```

### 影响

- 损坏、拼接或注入的尾随数据被静默接受；
- variant 等错误可能留下字节但仍被上层视为成功；
- RPC 参数签名、消息认证或审计逻辑可能对“同一语义的多种字节串”产生歧义；
- 难以发现协议 framing 错误。

### 修复方案

增加严格完成检查：

```cpp
bool finish() {
    return offset == size;
}
```

提供两个明确模式：

- `StrictSingleValue`：默认，必须完整消费；
- `Streaming`：显式允许连续读取多个值。

RPC payload 必须使用 strict 模式。

### 验收标准

- RPC 参数和返回值存在尾随字节时返回 InvalidParams/InvalidRequest；
- standalone API 默认拒绝尾随数据；
- 多值 stream API 通过独立类型或显式配置提供。

---

<a id="ser-010"></a>
## SER-010：重复 map key 和 set 元素被静默丢弃

- **优先级：P2**
- **证据：静态确认**
- **位置：**
  - `include/nekoproto/serialization/parsing/map.hpp:61-98,113-134`
  - `include/nekoproto/serialization/parsing/sequence.hpp:32-43,61-77`

### 原因

map 使用 `emplace()`，set/unordered_set 使用 `emplace()` 或 `insert()`，但没有检查返回值。重复元素被容器静默拒绝，parser 仍返回成功。

### 影响

- 输入与解析结果不是一一对应；
- 不同 backend 对重复 key 的处理可能不同；
- 签名验证、权限参数和配置合并场景可能产生安全歧义；
- schema 标记 `uniqueItems`，但 parser 没有执行验证。

### 修复方案

增加策略：

```cpp
enum class DuplicatePolicy {
    Reject,
    FirstWins,
    LastWins
};
```

安全默认值建议为 `Reject`。map 和 unique sequence 都应检查 insertion result。

### 验收标准

- 默认模式遇到重复 key/element 返回带路径的错误；
- 可选兼容模式行为有文档和测试；
- JSON 对象重复名称的行为与通用 map parser 规则一致。

---

<a id="ser-011"></a>
## SER-011：解析失败会留下部分修改后的目标对象

- **优先级：P2**
- **证据：静态确认**
- **位置：**
  - `include/nekoproto/serialization/parsing/sequence.hpp:61-77`
  - `include/nekoproto/serialization/parsing/map.hpp:61-134`
  - `include/nekoproto/serialization/parsing/reflection.hpp:242-254`

### 原因

容器在解析开始时直接 `clear()`，然后逐项插入。反射对象也直接写入目标字段。如果中途失败，调用方得到的是“部分新值”，旧值已经丢失。

### 影响

- 调用方容易误用部分有效、部分旧值或默认值的对象；
- 重试和错误恢复困难；
- 配置热更新时可能把运行状态留在不一致状态；
- RPC 参数解析失败后，调试信息可能受到部分写入影响。

### 修复方案

默认采用 parse-then-commit：

```cpp
T temporary{};
parse(temporary);
if (success) target = std::move(temporary);
```

大型容器可提供显式 `InPlace` 模式，但默认 API 应提供强错误保证。

### 验收标准

- 任意字段/元素解析失败后，目标值保持调用前状态；
- 对不可复制但可移动类型仍可使用临时值方案；
- 文档明确异常/错误保证。

---

<a id="ser-012"></a>
## SER-012：bool 和可变整数允许非规范编码

- **优先级：P2**
- **证据：静态确认**
- **位置：**
  - `include/nekoproto/serialization/binary/binary_reader.hpp:131-137,223-255`
  - `include/nekoproto/serialization/private/integer.hpp:115-152`

### 原因

bool Reader 把任意非零字节都视为 true。整数 Decoder 接受部分冗余 continuation 编码，signed 编码还可能出现“负号 + 零 magnitude”的负零等价形式。

### 影响

- 同一个逻辑值可能有多个 wire encoding；
- 对消息做签名、hash、缓存 key 或去重时产生歧义；
- malformed 数据未被及时拒绝；
- fuzz 和协议验证难以定义唯一规范。

### 修复方案

- bool 只接受 `0x00` 和 `0x01`；
- integer Decoder 检查最短编码；
- signed zero 只允许正零；
- 提供 `Canonical` 与显式 legacy-tolerant 模式，RPC 默认 canonical。

### 验收标准

- Writer 生成唯一编码；
- Reader 默认拒绝冗余 varint、负零和非法 bool；
- golden vectors 覆盖边界值和非法序列。

---

<a id="ser-013"></a>
## SER-013：`isEmpty()` 和容器初始化缺少空 InputValue 校验

- **优先级：P2**
- **证据：静态确认**
- **位置：**
  - `include/nekoproto/serialization/binary/binary_reader.hpp:40-43,108-119,190-220`

### 原因

`validate()` 会检查 `input.state == nullptr`，但 `isEmpty()` 和 `initializeContainer()` 在调用 validate 前直接解引用 `input.state`。`InputValue` 又允许默认构造出空 state。

### 影响

- 自定义 parser 或错误 API 使用可能造成空指针解引用；
- 同一 Reader 的基础类型与容器类型错误行为不一致；
- 公共扩展接口不够健壮。

### 修复方案

所有读取入口统一先调用 `validate()`。更进一步，可删除 `InputValue` 的公开默认构造，或让无效 handle 使用显式 factory 创建。

### 验收标准

- 空 handle 在所有类型上都返回 `Binary input handle is null`；
- ASan/UBSan 测试不发生崩溃；
- 自定义 parser 文档明确 handle 生命周期。

---

<a id="ser-014"></a>
## SER-014：`to_json_value()` 忽略序列化和反序列化错误

- **优先级：P2**
- **证据：静态确认**
- **位置：**
  - `include/nekoproto/serialization/json_serializer.hpp:32-41`
  - `include/nekoproto/jsonrpc/backend.hpp:212-245`

### 原因

辅助函数连续调用 `out(obj)`、`out.end()` 和 `in(json)`，但不检查任何返回值，最终总是返回一个 `JsonValue`。

JSON-RPC `appendSuccess()`/`appendError()` 使用该函数，并通过 `hasValue()` 决定是否追加响应。错误信息在中间已经丢失。

### 影响

- 返回值序列化失败时可能静默不发送响应；
- 服务器内部错误无法转换成明确的 JSON-RPC Internal error；
- 调试日志缺少真实失败原因。

### 修复方案

改为：

```cpp
Result<JsonValue, sa::Error> to_json_value(const T& value);
```

JSON-RPC backend 必须显式处理失败并生成 InternalError，或者将错误传播到 dispatcher。

### 验收标准

- 每一步错误均可观察和记录；
- JSON-RPC 返回值序列化失败会生成带相同 request id 的 InternalError；
- 不再通过空 JsonValue 隐式表示失败。

---

<a id="ser-015"></a>
## SER-015：通用 Serializer 的多值 API 在不同 backend 上语义不一致

- **优先级：P3**
- **证据：静态确认**
- **位置：**
  - `include/nekoproto/serialization/serializer_adapter.hpp:29-38,92-106`

### 原因

Adapter 支持：

```cpp
serializer(a, b, c);
```

Binary backend 会把多个值连续写入；JSON 类 backend 通常只有一个 root，后写值可能覆盖前值，Input 也可能重复从同一个 root 读取。

### 影响

- 用户认为通用 API 跨 backend 等价，但实际语义不同；
- 泛型代码切换 serializer 时可能静默改变输出；
- 与 SER-009 的完整消费规则冲突。

### 修复方案

- 单文档值 API 只允许一个 root；
- 多值应显式包装成 tuple/array/object；
- Binary streaming 使用独立 `BinaryValueStreamReader/Writer`。

### 验收标准

- 编译期或运行时拒绝文本 backend 的多 root 调用；
- 文档示例使用 tuple 或反射对象表达多值；
- streaming API 与单值 API 类型上可区分。

---

# 4. RPC 与传输层问题和修复列表

<a id="rpc-001"></a>
## RPC-001：多个连接共享 dispatcher 执行状态，request ID 和取消 handle 会跨连接冲突

- **优先级：P0**
- **证据：静态确认**
- **位置：**
  - `include/nekoproto/rpc/server.hpp:40-46,93-143`
  - `include/nekoproto/rpc/dispatcher.hpp:152-198,200-215,226-265`

### 原因

服务器为每个 endpoint 创建独立 `PeerSession`，但所有 endpoint 都调用同一个 `mDispatcher`。Dispatcher 的以下成员是全局共享的：

```cpp
std::vector<Id> mCurrentIds;
std::map<Id, ilias::StopHandle> mCancelHandles;
ilias::TaskGroup<void> mTaskScope;
```

取消表只使用 request ID 作为 key。不同客户端通常都会从 ID 1 开始，因此：

```text
client A request id = 1
client B request id = 1
```

后来的 handle 会覆盖前一个。

### 影响

- 取消错误客户端的请求；
- 某个 batch 完成后清空另一个连接的取消 handle；
- `getCurrentIds()` 混合不同连接请求；
- 一个连接可能等待另一个连接的 TaskGroup 任务；
- 多线程 IoContext 下存在数据竞争风险。

### 修复方案

拆分“方法注册表”和“执行状态”：

```cpp
struct RequestKey {
    SessionId session_id;
    RequestId request_id;
};
```

- handler registry 可只读共享；
- task group 应是连接级或 batch 级；
- active request registry 使用 `(session_id, request_id)`；
- 取消 API 应要求 session，或返回全局唯一 request token；
- 每个请求结束时只删除自己的 handle，禁止全表 `clear()`。

### 验收标准

- 两个客户端使用相同 ID 时互不影响；
- 取消 A/1 不会停止 B/1；
- 一个 batch 完成不清除其他连接状态；
- TSan 下多连接测试无竞态。

---

<a id="rpc-002"></a>
## RPC-002：共享 TaskGroup 和响应 vector 可能产生跨批次等待与并发写入

- **优先级：P1**
- **证据：高风险待验证**
- **位置：**
  - `include/nekoproto/rpc/dispatcher.hpp:164-186`
  - `include/nekoproto/rpc/dispatcher.hpp:226-258`

### 原因

`processMessage()` 把 method wrapper task spawn 到共享 `mTaskScope`；`_handle()` 又把实际用户任务 spawn 到同一个 scope。多个用户任务通过引用捕获同一个局部 `responses` vector，并在 finally 中追加结果。

如果 TaskGroup 允许并行执行，多个 `emplace_back()` 会产生 data race 和 vector 重分配导致的 UB。即使当前是单线程协作调度，跨 batch 共用 `waitAll()` 仍会造成不必要的耦合。

### 影响

- 响应 vector 损坏或崩溃；
- 响应顺序不确定；
- 一个请求批次等待其他批次；
- 调度器行为变化后出现隐藏回归。

### 修复方案

- 每个 batch 使用局部 TaskGroup；
- 为每个 request 分配固定 response slot；
- 任务结束后由单一协程汇总结果；
- 若协议要求保持 batch 顺序，按原 request index 编码；
- 避免嵌套 spawn 到同一个全局 scope。

### 验收标准

- 1000 个并发方法调用在 TSan 下无告警；
- 响应顺序规则明确且可测试；
- 不同 batch 的 wait 生命周期独立。

---

<a id="rpc-003"></a>
## RPC-003：`RpcClient::close()` 后仍被视为已连接

- **优先级：P1**
- **证据：静态确认**
- **位置：**
  - `include/nekoproto/rpc/client.hpp:43-70`
  - `include/nekoproto/rpc/client.hpp:148-158`

### 原因

`close()` 只调用 endpoint 的 `close()`，没有 `mEndpoint.reset()`。`isConnected()` 仅判断指针是否为空，`_callRemote()` 也只做同样检查。

### 影响

- `close()` 后 `isConnected()` 返回 true；
- 后续 RPC 尝试访问已关闭 endpoint；
- 调用方无法根据状态做可靠重连；
- close、shutdown 的语义不一致。

### 修复方案

- 引入明确状态：`Disconnected/Connecting/Ready/Closing/Closed`；
- `close()` 在同步保护下关闭并 reset endpoint；
- `isConnected()` 应反映 endpoint 和握手状态；
- close 应唤醒或取消所有 pending calls。

### 验收标准

```cpp
client.close();
EXPECT_FALSE(client.isConnected());
EXPECT_EQ(call_after_close.error(), ClientNotInitOrClosed);
```

---

<a id="rpc-004"></a>
## RPC-004：client endpoint 生命周期和 server endpoint 列表缺少统一同步

- **优先级：P1**
- **证据：高风险待验证**
- **位置：**
  - `include/nekoproto/rpc/client.hpp:43-92,148-229`
  - `include/nekoproto/rpc/server.hpp:52-103,104-143`

### 原因

Client 的 `_callRemote()` 使用 `mMutex`，但 `close()`、`shutdown()`、`setEndpoint()` 和 `isConnected()` 没有使用同一同步机制。Server 的 `mEndpoints` 会被 `addEndpoint()`、endpoint loop 的 `EraseGuard`、`close()`、`flush()` 和 `shutdown()` 访问，也没有明显锁或 actor 约束。

### 影响

在多线程 IoContext 或跨线程调用 API 时可能出现：

- 对 `unique_ptr` 的并发读写；
- use-after-free；
- list iterator/节点并发删除；
- peer session 与 endpoint 不匹配；
- shutdown 和 response send 竞争。

### 修复方案

二选一：

1. 明确所有对象只能在一个 executor/thread 上操作，并增加运行时断言；
2. 使用统一状态 mutex/strand/actor mailbox 保护所有生命周期操作。

Server endpoint slot 可改成 `shared_ptr<Connection>`，由连接对象自行管理生命周期，服务器只维护稳定 ID 到 weak/shared pointer 的表。

### 验收标准

- 并发 close/setEndpoint/call 的压力测试通过；
- TSan 无告警；
- API 文档明确线程安全等级。

---

<a id="rpc-005"></a>
## RPC-005：客户端所有调用被一把 mutex 完全串行化

- **优先级：P1**
- **证据：静态确认**
- **位置：**
  - `include/nekoproto/rpc/client.hpp:148-223`

### 原因

`_callRemote()` 在发送前获得 `mMutex`，并一直持有到收到、解析响应和完成可能的重试。协议虽然有 request ID，但客户端没有常驻 receiver 和 pending-request demultiplexer。

### 影响

- 同一连接只能有一个 in-flight request；
- 慢请求阻塞所有其他请求；
- request ID 的并发关联能力没有被使用；
- 长调用会放大超时和尾延迟；
- 无法实现高吞吐 RPC。

### 修复方案

采用标准 multiplexed client：

- send mutex 只保护 frame 写入；
- 一个常驻 receiver coroutine；
- `pending<RequestId, Promise/Channel>`；
- 收到 response 后按 ID 分发；
- 每个请求独立 deadline、cancel handle 和 retry state；
- 断线时一次性完成所有 pending 请求并返回错误。

### 验收标准

- 同一 client 可同时发出多个请求；
- 响应乱序到达时仍分配给正确调用；
- 慢请求不阻塞快请求；
- pending 数量有上限和背压。

---

<a id="rpc-006"></a>
## RPC-006：服务端等待当前请求完成后才继续读下一帧

- **优先级：P1**
- **证据：静态确认**
- **位置：**
  - `include/nekoproto/rpc/server.hpp:115-141`
  - `include/nekoproto/rpc/dispatcher.hpp:181-197`

### 原因

每个 endpoint loop 在读取一帧后：

```cpp
co_await mDispatcher.processMessage(...);
```

只有整个请求处理结束并发送响应后，才会再次 `recv()`。

### 影响

- 同一连接无法并行处理多个请求；
- 后续 cancel/control frame 无法及时读取；
- 长任务造成连接级 head-of-line blocking；
- 无法自然支持 streaming 或优先级控制。

### 修复方案

- recv loop 只负责解析和分发；
- 普通 request spawn 到连接级、有限并发的 task group；
- control/cancel frame 在 recv loop 中优先处理；
- send 由 endpoint 写锁序列化；
- 设置每连接最大 in-flight 数和队列长度。

### 验收标准

- 同一连接的短请求可在长请求之前完成；
- cancel frame 可在目标 handler 执行期间生效；
- 超过并发上限时有明确 backpressure/error。

---

<a id="rpc-007"></a>
## RPC-007：RPC frame 长度来自远端输入，分配前没有策略上限

- **优先级：P0/P1（安全）**
- **证据：静态确认**
- **位置：**
  - `include/nekoproto/rpc/private/backend_base.hpp:82-101`
  - `include/nekoproto/rpc/private/stream_endpoint.hpp:44-69`
  - `include/nekoproto/transport/endpoint.hpp:174-198`
  - `include/nekoproto/rpc/backend.hpp:65-73`

### 原因

Neko RPC header 包含 32-bit `method_size` 和 `payload_size`。Stream endpoint 计算 body size 后直接：

```cpp
buffer.reserve(header_size + body_size);
buffer.resize(header_size + body_size);
```

Options 中没有 `max_frame_bytes`。JSON-RPC 使用的通用 32-bit length-prefixed endpoint 同样直接按远端 size `resize()`。

### 影响

- 恶意 peer 可声明接近 4 GiB 的 frame，触发巨型分配；
- 进程可能 OOM、被系统杀死或长期阻塞读取；
- 多连接可放大内存占用；
- header 合法性检查不能替代资源策略限制。

### 修复方案

在任何分配和 body read 前检查：

```cpp
struct RpcLimits {
    uint32_t max_frame_bytes;
    uint32_t max_method_bytes;
    uint32_t max_payload_bytes;
    uint16_t max_extension_bytes;
};
```

建议不同 endpoint 共用统一 message size policy，并在超限后关闭连接或返回 MessageTooLarge。

### 验收标准

- 超限 header 不触发大内存分配；
- 默认限制为有限值；
- 单连接和全局同时有内存预算；
- 测试验证声明长度大于限制时立即失败。

---

<a id="rpc-008"></a>
## RPC-008：解压缩没有输出大小和压缩比预算

- **优先级：P1（安全）**
- **证据：静态确认**
- **位置：**
  - `src/rpc.cpp:635-660`
  - `include/nekoproto/rpc/private/backend_compression.hpp:97-142`

### 原因

RLE decompressor 根据控制字节不断向输出 vector 插入数据，没有检查：

- 最大解压后字节数；
- 最大压缩比；
- frame 总预算；
- 原始尺寸声明。

### 影响

- 小于原始数据的压缩 payload 可被放大数十倍；
- 与无 frame 上限组合时形成显著内存放大；
- 自定义替换压缩 codec 后风险可能更高。

### 修复方案

- `decompress()` 接收 `max_output_bytes`；
- 在每次 append/insert 前检查预算；
- frame extension 携带 original size，并在解压前验证；
- 限制 compression ratio；
- 统计超限次数并关闭恶意连接。

### 验收标准

- 超过输出预算时不继续扩容；
- 声明 original size 与实际结果不符时拒绝；
- 所有可替换 codec 都遵循相同限制接口。

---

<a id="rpc-009"></a>
## RPC-009：协议定义了 `Cancel` 帧，但服务端实际不会执行远程取消

- **优先级：P1**
- **证据：静态确认**
- **位置：**
  - `include/nekoproto/rpc/private/backend_base.hpp:29-35`
  - `include/nekoproto/rpc/backend.hpp:196-200`
  - `include/nekoproto/rpc/backend.hpp:545-562`
  - `include/nekoproto/rpc/server.hpp:126-140`
  - `include/nekoproto/rpc/dispatcher.hpp:200-213`

### 原因

Neko RPC 定义了 `Kind::Cancel`。但：

1. `handleServerControl()` 只识别 Hello；
2. Cancel 进入 `decodeIncoming()` 后仅返回 `ok=true`、零 requests；
3. Dispatcher 不会读取 Cancel 的 ID，也不会调用 `cancel()`；
4. Client 没有公开的 `cancelRemote(request_id)` 发送路径。

因此 Cancel 帧会被静默吃掉。

### 影响

- 协议宣称的远程取消功能不可用；
- 用户以为已取消，服务端任务仍继续运行；
- 长任务占用资源直到自然完成；
- 与服务端串行 recv 叠加后，即使补上 Cancel，也可能无法及时处理。

### 修复方案

- 将 Cancel 识别放在 server recv loop/control handler；
- 使用 `(session_id, request_id)` 查找 active request；
- 返回可选 cancel acknowledgement 或明确定义 fire-and-forget；
- Client `callRemote` 返回 request handle，或接受 `StopToken/deadline`；
- 目标不存在、已完成、重复取消要有明确语义。

### 验收标准

- 远端取消会停止正确 session 的正确任务；
- 不会取消其他连接的同 ID 请求；
- 取消在长任务执行期间可及时到达；
- 测试覆盖 cancel-before-start、during-run、after-finish。

---

<a id="rpc-010"></a>
## RPC-010：服务器关闭顺序可能等待在不可停止的 read 上

- **优先级：P1**
- **证据：高风险待验证**
- **位置：**
  - `include/nekoproto/rpc/server.hpp:52-58`
  - `include/nekoproto/rpc/private/stream_endpoint.hpp:44-69`
  - `include/nekoproto/transport/endpoint.hpp:184-197`

### 原因

`RpcServer::close()` 的顺序是：

1. `mScope.stop()`；
2. `wait().wait()`；
3. 最后才关闭 endpoint。

但 stream body read 使用 `ilias::unstoppable()`。若 scope stop 不能中断当前 read，`wait()` 可能一直等待，而真正能解除 read 的 endpoint close 尚未执行。

### 影响

- 析构或 close 卡死；
- 服务关闭、测试退出和热重启不可靠；
- 半包或永不发送 body 的 peer 可阻塞 shutdown。

### 修复方案

- 先停止接收新连接/请求；
- 关闭或取消所有 endpoint read；
- 再 stop task scope 并等待；
- 为 shutdown 设置 deadline；
- 避免对不可信网络 body read 使用完全 unstoppable 的等待，或提供 shutdown-aware read。

### 验收标准

- peer 只发送 header、不发送 body 时，server close 在限定时间内完成；
- 空闲连接、半包连接、活跃 handler 三种场景都有测试；
- shutdown 多次调用保持幂等。

---

<a id="rpc-011"></a>
## RPC-011：远端 method table 的稀疏超大 ID 可触发巨型 vector resize

- **优先级：P1（安全）**
- **证据：静态确认**
- **位置：**
  - `src/rpc.cpp:141-175`
  - `include/nekoproto/rpc/backend.hpp:695-725`
  - `src/rpc.cpp:423-440,561-572`

### 原因

远端 method table entry 的 ID 是 64-bit。解析后会调用：

```cpp
mEntries.resize(static_cast<size_t>(entry.id) + 1U);
```

只检查是否超过 `size_t`，没有限制最大 method ID、最大表项数量或 ID 是否连续。一个很小的 extension 可包含极大的 sparse ID。

### 影响

- 恶意 server/client 可触发超大 vector 分配；
- 握手或错误恢复阶段即可 OOM；
- exception 在部分 `noexcept` 路径上还可能导致 terminate。

### 修复方案

- 验证 `entry.id < max_method_entries`；
- 要求 full table ID 连续且从零开始；
- delta 只允许合理增长窗口；
- 或将存储改成 `unordered_map<uint64_t, Entry>`，但仍必须设置数量和 ID 限制；
- 解析 table 前验证 count、总字节和重复 ID/name。

### 实现级性能修正

`applyRemoteTable()` 原先先调用 `_validTable(entries, version, true)`，随后又调用公开的 `candidate.reset()`；后者会对相同 entries 再执行一次 `_validTable()`。验证包含全表扫描和 active name 去重表构造，并非 O(1)。

当前实现保留入口处的一次完整校验，再调用私有 `_resetValidated()` 安装已经验证的数据。公开 `reset()` 仍自行校验，因此没有扩大不安全 API；候选对象构造成功后才替换当前表，原子更新语义也不变。

### 验收标准

- sparse 大 ID 被拒绝且不分配大内存；
- 重复 ID、重复名称、非法 state 被拒绝；
- table/delta 有独立大小限制。
- `applyRemoteTable()` 对 full table 只执行一次 `_validTable()`。

---

<a id="rpc-012"></a>
## RPC-012：JSON-RPC parse error、空 batch 和非法 batch item 处理不完整

- **优先级：P1**
- **证据：静态确认**
- **位置：**
  - `include/nekoproto/jsonrpc/backend.hpp:93-132`
  - `include/nekoproto/rpc/dispatcher.hpp:152-160`

### 原因

- JSON 解析失败时 `decodeIncoming()` 返回 `ok=false`，dispatcher 只记录日志并返回空响应；
- 空数组 `[]` 被视为 batch，但没有 request 和 response，最终不响应；
- batch 中某个 item 解析失败或版本错误时使用 `break`，既不为非法 item 生成错误，也不继续处理后续合法 item；
- 非 object batch item 同样导致提前停止。

### 影响

- JSON-RPC 客户端可能永久等待 parse error/invalid request 响应；
- 混合 batch 中后续合法请求被无声丢弃；
- 行为与常见 JSON-RPC 2.0 实现不兼容；
- 容易造成协议互操作问题。

### 修复方案

Backend decode result 应能携带“无法形成 DecodedRequest 的协议错误响应”：

- Parse error：`id = null`；
- 空 batch：单个 Invalid Request；
- batch 非法 item：为该 item 添加 Invalid Request，继续处理其他 item；
- notification 的方法级错误不返回响应，但结构本身非法时按规范处理。

### 验收标准

测试覆盖：

```text
invalid JSON
[]
[1]
[valid, invalid, valid]
wrong jsonrpc version
missing method
```

并验证每个响应的 ID、error code 和 batch 数量。

---

<a id="rpc-013"></a>
## RPC-013：JSON-RPC response 未验证 `result` 与 `error` 的互斥和完整性

- **优先级：P1**
- **证据：静态确认**
- **位置：**
  - `include/nekoproto/jsonrpc/backend.hpp:311-334`
  - `include/nekoproto/jsonrpc/jsonrpc_traits.hpp` 中的 response optional 字段定义

### 原因

response 解析后：

- 有 error 就直接返回 error；
- 无 error 且非 void 时直接调用 `response.result.value()`；
- 没检查 result/error 是否“恰好存在一个”。

当 response 两者都没有时，`optional::value()` 会抛 `std::bad_optional_access`。当两者都有时，错误被优先使用，但非法响应没有被拒绝。

### 影响

- 恶意或损坏响应可能通过异常终止 coroutine/进程；
- 非法协议数据被当成合法响应；
- void response 的结构验证更宽松，可能掩盖 server bug。

### 修复方案

解析后执行统一验证：

```text
jsonrpc == "2.0"
id present and matches
exactly one of result/error is present
error object fields valid
```

所有失败返回 `InvalidResponse`/`ParseError`，禁止在网络输入路径直接使用 unchecked `.value()`。

### 验收标准

- missing both、present both、missing id、wrong version 均返回错误而不抛异常；
- void 和非 void 方法遵循相同 response envelope 规则。

---

<a id="rpc-014"></a>
## RPC-014：大量可能分配或执行用户代码的函数被标记为 `noexcept`

- **优先级：P2**
- **证据：静态确认**
- **位置示例：**
  - `include/nekoproto/rpc/client.hpp:43-149`
  - `include/nekoproto/rpc/server.hpp:52-207`
  - `include/nekoproto/rpc/dispatcher.hpp:104-123,218-258`
  - `include/nekoproto/rpc/method.hpp` 多处赋值和 function 包装

### 原因

例如 `setEndpoint()` 标记 `noexcept`，内部却调用 `std::make_unique`。方法注册会构造 string、map、vector 和 `std::function`。这些操作在异常启用构建中都可能抛出 `std::bad_alloc` 或用户类型异常。

### 影响

- 发生异常时直接 `std::terminate()`；
- RPC handler 抛出的异常可能越过边界；
- 内存压力下服务无法优雅返回 InternalError。

### 修复方案

- 移除不真实的 `noexcept`；
- 在 RPC 用户代码边界捕获所有异常，转换为 InternalError；
- 区分 allocation-free 的 close flag 操作和可能分配的注册/重连操作；
- 在无异常构建中保持 Result-based 路径。

### 验收标准

- 模拟 handler 抛异常时 server 不退出；
- 模拟 allocator failure 时 API 不 terminate，或文档明确进程策略；
- clang-tidy 检查 `noexcept` 与潜在 throwing calls。

---

<a id="rpc-015"></a>
## RPC-015：缺少请求 deadline、等待超时、全局并发限制和背压策略

- **优先级：P2**
- **证据：静态确认**
- **位置：**
  - `include/nekoproto/rpc/client.hpp:148-223`
  - `include/nekoproto/rpc/server.hpp:104-143`
  - `include/nekoproto/rpc/dispatcher.hpp:152-198`

### 原因

当前调用等待 send/recv 和 handler 完成，没有通用 deadline。Server 也没有明确的：

- 每连接最大 in-flight；
- 全局最大 active requests；
- pending response byte budget；
- 排队策略；
- overload error。

### 影响

- 丢失响应或恶意 peer 可让调用无限等待；
- 慢客户端/慢 handler 消耗连接和任务资源；
- 高负载时内存不可控；
- shutdown 需要依赖外部强制停止。

### 修复方案

新增：

```cpp
RpcCallOptions {
    deadline;
    timeout;
    priority;
    cancellation_token;
}
```

Server options 增加每连接和全局限制。超过限制时返回 Busy/Overloaded，或应用明确的排队/拒绝策略。

### 验收标准

- 超时后 pending entry 被清理；
- late response 不会错误匹配新请求；
- 达到并发上限时资源保持稳定；
- 指标包含 active、queued、timed_out、canceled、rejected。

---

# 5. 测试、Fuzz 与 CI 问题

<a id="test-001"></a>
## TEST-001：Binary serializer 缺少关键回归矩阵

- **优先级：P1**
- **证据：静态确认**
- **位置：**
  - `tests/unit/serializer/test_binary_serializer.cpp`
  - `tests/TEST_SCOPE.md`

现有 Binary 测试覆盖结构体、非字符串 map key、pair、unframed、fixed length 和截断输入，但没有覆盖本报告中已复现的高风险组合。

### 需要增加

- null 与字符串 `"null"`；
- binary variant；
- 空 optional 位于对象不同位置；
- flat 多字段；
- 未知 enum raw value；
- 尾随字节；
- 浮点 golden endian；
- 超大长度和深度限制；
- 重复 key/element；
- 非规范 bool/varint；
- 字段重排、未知字段和版本兼容。

---

<a id="test-002"></a>
## TEST-002：RPC 缺少多连接、取消、关闭和竞态测试

- **优先级：P1**
- **证据：静态确认**
- **位置：**
  - `tests/unit/rpc/test_neko_rpc_backend.cpp`
  - `tests/unit/jsonrpc/test_jsonrpc.cpp`

现有 RPC 测试主要验证单连接调用、method-id、压缩、通知和错误返回，没有覆盖 dispatcher 全局状态问题。

### 需要增加

- 两个客户端同时使用同一 request ID；
- 跨连接取消隔离；
- 同一连接多个 in-flight 请求；
- response 乱序；
- close during call；
- server close during partial frame；
- Cancel frame 生效；
- method table sparse ID；
- 超大 frame 和解压预算；
- handler 抛异常；
- TSan 多线程 IoContext。

**2026-07-15 实施结果：**上述可观察行为已纳入 RPC 40 个普通用例，TSan 下同一目标 40/40 通过。

---

<a id="test-003"></a>
## TEST-003：Fuzz target 基本只覆盖 JSON proto

- **优先级：P1/P2**
- **证据：静态确认**
- **位置：**
  - `tests/fuzz/proto/test_json_proto.cpp`

### 建议新增 fuzz target

```text
fuzz_binary_reader
fuzz_binary_roundtrip
fuzz_binary_canonical_encoding
fuzz_jsonrpc_decode
fuzz_neko_rpc_frame
fuzz_method_table_tlv
fuzz_rpc_compression
fuzz_length_prefixed_endpoint_header
```

每个 target 应设置输入上限、timeout 和内存限制，并在 ASan/UBSan 下运行。对于 parser，增加“不崩溃、不超预算、成功时完整消费、重新编码满足 canonical”不变量。

**2026-07-15 实施结果：**保留一个 JSON proto target，并用一个有界 RPC/wire target 合并覆盖 Binary Reader、
Neko RPC frame、method-table TLV 与压缩解码，避免为每个解码器复制 target；两者在 ASan+UBSan 下各完成
1000 次 smoke。

---

<a id="ci-001"></a>
## CI-001：构建工具使用 `latest`，Actions 未固定 commit，权限未最小化

- **优先级：P2**
- **证据：静态确认**
- **位置：**
  - `.github/workflows/xmake-test-on-linux.yml`
  - `.github/workflows/xmake-test-on-windows.yml`
  - `.github/workflows/xmake-test-clang-cl.yml`
  - `.github/workflows/issue-translator.yml`

### 问题

- `actions/checkout@v2` 较旧；
- xmake action 使用 `@v1` 且 `xmake-version: latest`；
- 其他 action 使用可移动 tag；
- workflow 没有显式最小化 `permissions`；
- 相同 commit 在不同时间可能使用不同 xmake 版本得到不同结果。

### 修复方案

- 固定 xmake 具体版本；
- Actions 固定完整 commit SHA；
- 默认：

```yaml
permissions:
  contents: read
```

- 仅 issue translator job 授予必要的 `issues: write`；
- 使用 Dependabot/Renovate 定期更新固定 SHA。

---

<a id="ci-002"></a>
## CI-002：没有独立的 sanitizer、协议 golden vector 和兼容性作业

- **优先级：P1/P2**
- **证据：静态确认**

### 建议 CI 矩阵

1. ASan + UBSan：序列化、frame、method table；
2. TSan：RPC 多连接和 lifecycle tests；
3. libFuzzer smoke corpus；
4. GCC/Clang/MSVC；
5. Debug/Release，shared/static；
6. Binary V1/V2 golden vectors；
7. 旧版本数据由新版本读取；
8. 使用 qemu 或字节级 golden 测试验证 endian 独立性。

覆盖率 job 目前配置为 coverage，但生成报告步骤被注释，应确认是否确实产出可用覆盖率数据。

**2026-07-15 实施结果：**coverage 生成、ASan/UBSan/TSan、fuzz smoke 和 Binary golden 已恢复并通过本地
专项复验；旧版本数据兼容作业尚未新增，继续由 CI-002 跟踪。

---

# 6. 推荐实施顺序

## 阶段 0：先锁定当前行为并建立回归测试

在修改实现前，先把已经确认的问题写成 failing tests。这样可以避免 Binary V2 和 RPC 重构过程中引入新的不兼容。

建议首先加入：

```cpp
TEST(Binary, OptionalStringNullIsNotNullopt);
TEST(Binary, VariantPreservesActiveIndex);
TEST(Binary, EmptyOptionalDoesNotCorruptObjectFrame);
TEST(Binary, FlatObjectWritesCorrectMemberCount);
TEST(Binary, UnknownEnumUnderlyingValueRoundTrips);
TEST(Binary, RejectsTrailingBytesInStrictMode);
TEST(Rpc, SameRequestIdOnDifferentSessionsIsIsolated);
TEST(Rpc, CancelFrameStopsTargetRequest);
TEST(Rpc, OversizedFrameIsRejectedBeforeAllocation);
```

## 阶段 1：修复 Binary 正确性并设计 V2

1. 冻结并记录 V1 golden vectors；
2. 设计 V2 magic/version/type tag/TLV；
3. variant 写 index；
4. null 使用独立 tag；
5. float 使用规范字节序；
6. dynamic/flat/optional 成员数量正确；
7. strict complete-consumption；
8. parse limits 和 canonical encoding。

不建议继续给 V1 叠加大量特殊规则后仍称其为同一协议版本。

## 阶段 2：隔离 RPC session、batch 和 request 状态

1. Handler registry 与 execution state 分离；
2. 为连接分配稳定 SessionId；
3. active request key 改为 `(session, request)`；
4. 每 batch/connection 使用独立 TaskGroup；
5. 响应通过固定 slot 或安全 collector 汇总；
6. 实现真正可达的 Cancel 控制路径。

## 阶段 3：重构客户端 multiplexing 和生命周期

1. 常驻 receiver；
2. pending map；
3. send-only mutex；
4. deadline/cancellation；
5. close/reset 状态机；
6. 断线完成所有 pending；
7. server recv 与 handler 执行解耦。

## 阶段 4：安全加固

1. frame/message/payload/method 上限；
2. 解压输出预算；
3. method table 数量与 ID 限制；
4. parser 深度和分配预算；
5. duplicate 和 canonical 策略；
6. 异常边界。

## 阶段 5：JSON-RPC 合规和工程质量

1. parse error 和 invalid request response；
2. batch 逐项处理；
3. result/error envelope 验证；
4. sanitizer/fuzz/golden CI；
5. 固定工具链和 Action SHA。

---

# 7. 当前格式说明与目标架构决策

## 7.1 Binary V2 wire format：逐字节构成与解读

本节描述当前 `binary::Writer`/`binary::Reader` 已实现的 Binary V2，而不是未来草案。实现中的同一份说明位于 `binary_writer.hpp` 的 `ValueTag` 定义前。

<a id="binary-wire-byte-layout"></a>
### 7.1.1 普通文档外层

```text
Document := Magic RootValue
Magic    := 4E 50 02
            N  P  V2
```

- `4E 50` 是格式魔数；
- `02` 是 wire version；
- 普通 serializer 只允许一个 `RootValue`；
- Reader 先验证 magic，再从根 value 的首字节开始按类型递归解析；
- 根 value 解析完成后必须正好到达输入末尾，尾随或未消费字节会使 `finish()` 失败。

`raw_fixed_data` 是唯一不带该头部的显式例外，详见 7.1.7。

### 7.1.2 `ValueTag` 表

每个普通 value 的第一个字节都是 wire `ValueTag`。这里的 wire tag 是真实写入数据的字节，不等同于 C++ metadata 中的 `BinaryTag`/`ParserTag`。

| 首字节 | `ValueTag` | 后续 payload | 逐字节解释 |
| --- | --- | --- | --- |
| `00` | `Null` | 无 | null 到此结束 |
| `01` | `False` | 无 | bool false |
| `02` | `True` | 无 | bool true |
| `03` | `SignedInteger` | canonical ULEB128 | 先解 ULEB128，再做 zigzag 逆变换 |
| `04` | `UnsignedInteger` | canonical ULEB128 | 直接解为无符号整数 |
| `05` | `Float32` | 4 bytes | IEEE-754 bit pattern，大端序 |
| `06` | `Float64` | 8 bytes | IEEE-754 bit pattern，大端序 |
| `07` | `String` | `ULEB128(n) + n bytes` | 长度后紧跟原始字符串字节；binary 层不验证 UTF-8 |
| `08` | `Array` | `ULEB128(n) + n Values` | 元素依次递归解码 |
| `09` | `NamedObject` | `ULEB128(n) + n NamedFields` | 每个字段保留完整名称 |
| `0A` | `IdObject` | `ULEB128(n) + n IdFields` | schema 已知字段可使用压缩 key |
| `0B` | `FixedSigned8` | 1 byte | 定长有符号整数 |
| `0C` | `FixedSigned16` | 2 bytes | 定长有符号整数，大端序 |
| `0D` | `FixedSigned32` | 4 bytes | 定长有符号整数，大端序 |
| `0E` | `FixedSigned64` | 8 bytes | 定长有符号整数，大端序 |
| `0F` | `FixedUnsigned8` | 1 byte | 定长无符号整数 |
| `10` | `FixedUnsigned16` | 2 bytes | 定长无符号整数，大端序 |
| `11` | `FixedUnsigned32` | 4 bytes | 定长无符号整数，大端序 |
| `12` | `FixedUnsigned64` | 8 bytes | 定长无符号整数，大端序 |

当前格式没有为 variant 单独分配 `ValueTag`；Binary variant 由通用 `Array` 表达为 `[alternative-index, payload]`。

### 7.1.3 ULEB128 与 zigzag

ULEB128 的每个字节低 7 bit 保存数据，最高 bit 为 `1` 表示还有后续字节。Reader 只接受最短表示。

例如无符号整数 `300`：

```text
300 = 0b 00000010 00101100

AC 02
^^ ^^
|  +-- 最后 7 bit，最高位为 0，结束
+----- 最低 7 bit 为 0x2C，最高位为 1，继续
```

所以完整根值是：

```text
4E 50 02 | 04 AC 02
magic      tag uint(300)
```

有符号整数先 zigzag：

```text
 0 -> 0
-1 -> 1
 1 -> 2
-2 -> 3
```

因此 `int(-2)` 的完整根值为 `4E 50 02 03 03`。

### 7.1.4 Array、NamedObject 和自描述跳过

Array 的 grammar：

```text
Array := 08 ULEB128(element-count) Value...
```

`[true, uint(300)]`：

```text
4E 50 02 | 08 02 | 02 | 04 AC 02
magic      array2   true  uint300
```

NamedObject 的 grammar：

```text
NamedObject := 09 ULEB128(field-count) NamedField...
NamedField  := ULEB128(name-byte-count) name-bytes Value
```

`{"x": int(1)}`：

```text
4E 50 02 | 09 01 | 01 78 | 03 02
magic      object1  "x"    int1 (zigzag 2)
```

容器保存成员数量，每个子 value 又由自身 tag 和递归结构决定结束位置。因此 Reader 可以在不知道 C++ 目标字段的情况下先构造节点边界，并跳过未知字段；无需给每个 scalar 再写一份 payload length。代价是：遇到未知 `ValueTag` 时无法推导它的长度，新增 tag 必须升级 wire version，不能静默猜测。

### 7.1.5 `IdObject` 的 compact field key

IdObject 的 grammar：

```text
IdObject := 0A ULEB128(field-count) IdField...
IdField  := ULEB128(key) [name-bytes] Value

key bit 0 == 0:
  name-byte-count = key >> 1
  后面存在 name-byte-count 个名称字节

key bit 0 == 1:
  field-hash = key >> 1
  后面没有名称字节
```

Writer 计算 `FNV-1a-32(name)`，但只有当 `ULEB128((hash << 1) | 1)` 比 `ULEB128(name.size << 1) + name bytes` 更短时才使用 hash；否则仍写 literal name。最低 bit 让两种 key 不会在 wire 上混淆。

短字段 `x` 通常保留名称：

```text
4E 50 02 | 0A 01 | 02 78 | 03 02
magic      idobj1   key=2   int1
                     | +--- name byte 78 ('x')
                     +----- even: name length = 2 >> 1 = 1
```

hashed key 只能由持有同一 schema 字段名的 Reader 重新计算并查找，不能还原原字段名；这正是 `IdObject` 不能替代动态 `Object` 的原因，详见 7.2。

### 7.1.6 Binary variant 的当前字节形态

当前通用 variant parser 默认统一写 `[alternative-index, payload]`；Binary 不再通过 backend 私有的
`explicit_variant_index` 开关决定该语义，所以：

```text
std::variant<int, std::string>{std::in_place_index<1>, "A"}

4E 50 02 | 08 02 | 04 01 | 07 01 41
magic      array2   index1  string "A"
```

解码时必须先验证 array 恰好有两个元素，再读取 index，并只按该 alternative 的类型解析第二个元素。alternative 顺序是 wire ABI；在没有稳定 alternative ID 之前，只能在末尾追加类型，不能重排或在中间插入。

### 7.1.7 `raw_fixed_data`

`BinaryTag{.raw_fixed_data = true}` 用于根级、命名反射对象的固定布局记录：

```text
RawDocument := FixedField FixedField ...
```

它不写 magic、version、`ValueTag`、字段名/ID、字段数或字段长度。每个未 ignore 字段都必须是带 `fixed_length` 的 arithmetic 或 enum；整数、enum、float/double 使用大端序，bool 只能是 `00`/`01`。

例如字段依次为 `uint32_t(0x01020304)`、`int32_t(-2)`、`uint16_t(0x1234)`：

```text
01 02 03 04 | FF FF FF FE | 12 34
uint32         int32          uint16
```

该模式适合通信头等双方已有精确 schema 的定长结构，不具备自描述、字段跳过或 schema 演进能力。字段顺序、数量、类型或宽度变化都必须当作独立协议版本处理。

### 7.1.8 Metadata tag 对 Binary 数据的实际影响

| metadata | 是否改变 Binary bytes | 当前含义 |
| --- | --- | --- |
| `BinaryTag.fixed_length` | 是 | integral 从 `03/04 + varint` 改为 `0B..12 + fixed big-endian`；宽度必须等于 C++ 类型宽度。bool、float/double 本来就是固定 payload，编码形态不额外变化 |
| `BinaryTag.raw_fixed_data` | 是，且改变整个根 grammar | 使用 7.1.7 的无头定长布局；只允许根级命名反射对象 |
| `rename_tag` | 是 | 改变 object field 的名称；对 `IdObject` 也会改变由名称计算的 hash，因此 rename 是 wire breaking change |
| `serialization_ignore_tag` | 是 | 字段完全不输出，同时减少 object member count |
| `ParserTag.flat` | 是 | 仅对命名反射对象字段生效：去掉子对象边界，把子字段合并进父对象，并按实际输出数重算 count |
| `ParserTag.skippable` | 否 | 只改变读取缺失字段时的策略；不改变已有字段的 bytes |
| `JsonTag.raw_string` | 否 | 仅对支持 raw JSON value 的 backend 有意义 |
| comment、YAML style/tag/anchor、TOML style | 否 | Binary backend 不消费这些展示类 metadata |

必须区分“tag 没有形成额外字节”和“tag 没有影响编码”：例如 `fixed_length` 本身不会被序列化成一个选项字段，但它会让 Writer 选择完全不同的 wire `ValueTag`。

<a id="object-idobject-contract"></a>
## 7.2 `Object` 与 `IdObject`：存在理由、边界与优势

### 7.2.1 它们不是两个业务对象类型

`parsing::Parent<W>::Object` 和 `IdObject` 是 parser 到 writer 之间的父节点句柄。两者都能承载 null、scalar、array 和嵌套 object，所以大量调用使用相同接口是合理的；真正不同的只有“写一个字段之前如何编码 field key”。

```text
Object
  field key = 完整名称

IdObject
  field key = 短名称，或在更省空间时使用名称的 FNV-1a 32-bit hash
```

当前只有 Binary Writer 暴露 `OutputIdObjectType`。反射 parser 看到 writer 支持该能力时，命名反射对象使用 `addIdObject()`；其他 backend 自动退回普通 `Object`，因此它是一个 backend capability，而不是通用数据模型分叉。

### 7.2.2 为什么动态 `Object` 不能直接全部替换为 `IdObject`

普通 `Object` 用于 string-key map、pair 和 backend 原生 mapping。尤其 string-key map 的 Reader 需要 `forEachObjectMember()` 恢复原 key；如果 wire 只剩 hash，原字符串不可逆，无法构造 map。

反射对象则由本地 schema 提供预期字段名。Reader 查字段时可以用同一名称重新计算 hash，不依赖 wire 中保留名称。因此只在 schema-known object 上启用压缩是合理边界。

### 7.2.3 优势

- 长反射字段名可缩短为最多 32-bit hash 的 ULEB128，重复对象较多时节省明显；
- 短字段名在 literal 更小时仍保留名称，不会为了“使用 ID”反而膨胀；
- Reader 为 name 和 hash 各建一次索引，反射字段查找为平均 O(1)，不会逐字段反复扫描对象；
- 与普通 `Object` 共享 child-value 接口，container/scalar parser 不需要复制两套实现；
- `ValueTag::NamedObject`/`IdObject` 明确区分能力，Reader 不会把动态 map 当作 hash object 枚举。

### 7.2.4 差异不是偶然，应形成显式契约

| 行为 | `Object` | `IdObject` |
| --- | --- | --- |
| wire tag | `09` | `0A` |
| 字段 key | 完整名称 | literal 名称或 hash key |
| `objectField(name)` | name index | 按同一压缩规则选择 name/hash index |
| `forEachObjectMember()` | 支持 | 不支持，因为 hashed field 不可恢复原名称 |
| 动态 string-key map | 支持 | 不适用 |
| 反射 schema 查字段 | 支持 | 支持且通常更紧凑 |
| 同一对象内重复/碰撞检测 | 重复 name | 重复 name、重复 hash；Writer 也检查本次输出内的 hashed collision |

接口相同的部分代表“如何把 value 放进对象”这一共同能力；规则不同的部分代表“field key 是否可逆”这一核心差异，不应为了表面统一而抹平。

### 7.2.5 当前限制与后续命名建议

当前所谓 ID 是由字段名计算的 32-bit FNV-1a hash，并不是 schema 中显式分配、跨 rename 稳定的 field ID：

- rename 会改变 hash，属于 wire breaking change；
- Writer 能发现同一份输出里两个已发射字段的 hash 碰撞，但 wire 自身不能证明某个 hash 对应哪一个原始名称；
- 新旧 schema 的不同字段若发生同 hash，旧端可能把远端字段误认为本地字段；32-bit 空间不能把该概率当作数学上为零；
- hashed field 无法用于需要枚举原名称的通用 object API。

因此短期保留该优化是合理的，但文档和 API 不应把它宣传为“稳定 field ID”。若继续演进，建议：

1. 将内部能力命名改为 `SchemaObject`/`KeyedSchemaObject`，比 `IdObject` 更准确；
2. schema 支持显式稳定 numeric field ID，并在编译/注册时检测冲突；
3. FNV 模式只作为未声明稳定 ID 时的 compact-name fallback；
4. golden vectors 固定 literal/hash 选择规则，避免启发式变化造成无版本 wire 变化。

<a id="tag-propagation-contract"></a>
## 7.3 Tag 传播：Writer/Reader 对称性审计与契约

### 7.3.1 实施前的不对称与当前结果

用户观察到的“不对称”来自两个层次混在一起：

1. Writer 通过 `Parent<W>::addArray/addObject/addValue(..., tags)` 优先调用 backend 的 tag-aware overload；
2. Reader 原先直接调用无 tag 的 `toArray()/toObject()/toBasicType()`，tag 主要由 parser 自己读取。

当前已增加 `serialization/parsing/reader.hpp` 作为 Reader 侧统一适配层。每个 helper 都优先选择
backend 的 tag-aware overload，并在该重载不存在时回退到旧的无 tag 接口。因此现有 Binary/JSON/Proto 等
legacy Reader 不需要同步改接口，新 backend 则可以选择在读侧验证或消费 node tag。

逐类审计如下：

| 类型类别 | write 向 payload 传 tag | read 向 payload 传 tag | 当前结果 |
| --- | --- | --- | --- |
| arithmetic/string/enum | 当前 value 直接消费 | `reader_to_basic`/fixed/raw/string-view 将当前 tag 交给 Reader | 对称；`fixed_length`、`raw_string` 仍由 parser 保证语义 |
| `optional<T>`、`shared_ptr<T>`、`unique_ptr<T>` | 非空时传给 `T` | null 检查和非 null payload 都使用当前 tag | 对称；透明 wrapper 不建立额外非 null node |
| `variant<T...>` | envelope/active payload 按 union scope 分派 | 同一 scope 规则，array 转换也经过 Reader adapter | 对称；`UnionTag` 只在当前 union 层消费 |
| vector/set/array | tag 只用于 array node；element 不继承 | `reader_to_array` 收到 tag；element 使用 `NoTags` | 对称；不会把容器 tag 污染到元素 |
| map/tuple/pair | tag 只用于外层 node；内部 key/value/element 不继承 | 外层 object/array 收到 tag；内部使用 `NoTags` | 对称 |
| reflected object | 对象 tag 用于 object node；字段使用自己的 tags | object conversion、field lookup、null/value conversion 均有对应分派点 | `rename/ignore/flat/skippable` 与 node tag 路径对称 |

### 7.3.2 会不会有不好的影响

实施前，Binary/JSON 的核心 round-trip 没有因为 `toArray()/toObject()` 不接 tags 而直接失配：类型结构已经在输入节点中，`fixed_length` 等语义 tag 也由 scalar parser 在两侧显式处理。

但该不对称会产生三类长期问题：

- backend-specific 的读侧语义无法实现。例如 YAML Writer 可以应用显式 YAML tag，而 Reader 没有机会验证预期 tag；
- 新增 container-level 语义 tag 时，Writer 可能悄悄生效，Reader 却只能忽略，形成单向格式；
- variant 同一份 tags 同时接触 envelope 和 payload，却没有属性级 scope，展示样式或未来 union tag 可能被应用到错误层级。

纯输出属性不要求“读回同样样式”，例如 comment、YAML flow/block style 和 anchor；它们可以在 Reader 中明确 no-op。对称性的目标是两侧经过相同的 tag 分派点并由 backend 明确决定，而不是强迫所有展示信息参与语义校验。

### 7.3.3 目标传播规则

不要采用“所有 tag 无条件递归传给所有后代”。这会让 `vector<int>` 字段上的一个 tag 同时表示“修饰 vector”与“修饰每个 int”，也会让 `flat`、rename、comment 等字段边界属性污染深层值。

统一把类型分成三类：

1. **透明 wrapper**：`optional`、智能指针。自身没有非 null wire node，payload-relevant tags 在 write/read 两侧都穿透一次；
2. **实体 container**：sequence、map、tuple、pair、reflected object。tag 默认只作用于当前 node，元素/字段使用自己的 tags；若要修饰全部元素，应提供显式 element tag wrapper，而不是隐式穿透；
3. **union wrapper**：variant/未来 tagged union。它既有 discriminator/envelope，又恰好有一个 active payload，必须按属性 scope 拆分：union encoding tag 由 wrapper 消费，payload representation tag 传给 active alternative，字段边界 tag 不继续传播。

已为 Reader 增加与 `Parent<W>` 对应的适配层，并保留旧 backend 的无 tag fallback：

```cpp
reader_to_array<R>(in, tags);       // 优先 R::toArray(in, tags)
reader_to_object<R>(in, tags);      // 优先 R::toObject(in, tags)
reader_to_basic<R, T>(in, tags);    // 优先 R::toBasicType<T>(in, tags)
reader_object_field<R>(obj, name, tags);
```

适配层还覆盖 fixed basic、raw string、string view、`isEmpty()` 和 object member 遍历。这样 parser 在
write/read 两侧都把“当前 node 的 tags”交给 backend。旧 Reader 继续编译；Binary/JSON 当前通过 fallback
保持原行为；YAML 等 backend 可以逐步实现验证，而不需要再次修改通用 parser。

实现中的传播边界是显式的：sequence/map/tuple/pair 仅在创建或转换外层 node 时传 tag，元素和内部
`key/value` 不继承；optional/pointer 在非 null 时把 tag 交给实际 payload；reflection 把对象自身 tag 与字段
tag 分开，字段 tag 只进入该字段的 lookup/null/value 路径；variant 通过 `unionPayloadTags()` 消费当前层的
`UnionTag`，不会让外层 untagged 策略污染嵌套 union。

### 7.3.4 属性 scope

| scope | 典型属性 | 传播规则 |
| --- | --- | --- |
| field boundary | rename、ignore、skippable、flat、comment | 在 reflection/field parser 消费，不传给普通 child |
| node representation | `fixed_length`、`raw_string`、YAML scalar/collection style | 交给实际承载该 node 的 parser/backend |
| transparent payload | optional/pointer 内部非 null value | write/read 同时穿透一次 |
| union envelope | union discriminator/encoding、envelope collection style | 只由 union wrapper 消费 |
| union payload | active alternative 的 scalar/object representation | 过滤掉 union/field 属性后传给 active alternative |

当前实现采用等价的结构化分派：容器 parser 在 child 调用处显式改用 `NoTags`，透明 wrapper 显式透传，
reflection 在 object/field 两层分别分派，union 使用 `unionPayloadTags()` 过滤当前层策略。若以后新增可同时
携带多种 envelope/payload 属性的 union tag，再把该 helper 扩展为属性级 trait，而不是重新引入全量递归透传。

### 7.3.5 对称性验收矩阵

- `optional<int>` + `fixed_length` 的 present/null 两种状态；
- `variant<int, string>` + payload representation tag；
- `vector<T>` 的 node tag 不污染 element；
- `optional<vector<T>>` 的 node tag 穿过 optional 后只作用于 vector；
- reflected field 的 rename/flat/skippable 在 write/read/schema 三条路径一致；
- tag-aware Reader 和 legacy Reader backend 都可编译；
- output-only tag 在 Reader 明确 no-op，不得悄悄改变解析结果。

<a id="union-variant-contract"></a>
## 7.4 Union/variant 的统一处理范式

### 7.4.1 默认策略

当前实现统一采用：

```text
Union := [alternative-index, payload]
```

这不仅是 Binary 的特殊补丁，而应成为 `std::variant` 和未来 union-like custom type 的通用 parser 语义。优点是：

- 不再依赖“按 alternative 顺序试到第一个成功”的歧义行为；
- Reader 不需要为正常路径做 checkpoint/rollback；
- scalar、object、container alternatives 使用同一 envelope；
- write/read/schema 可以共享 discriminator 规则。

该规则已经上移到 `serialization/parsing/variant.hpp`，所有使用通用 variant parser 且支持 array 的 backend
共享同一默认值；Binary Writer/Reader 原先的 `explicit_variant_index` 私有开关已经删除。JSON、Binary 与
Proto 回归均已切换到该形态，JSON-RPC ID 这类协议明确规定为 scalar union 的类型仍通过自己的专用 parser
保持协议格式，不受应用层 variant 默认值影响。

这是对原先文本 backend 裸 payload 格式的 wire breaking change。需要读取旧数据的字段必须显式标记
`UnionEncoding::Untagged`，新旧进程混部时也必须先完成协议协商或同步升级，不能依赖 Reader 自动猜两种形态。

### 7.4.2 不复用 `flat`

不建议用现有 `ParserTag.flat` 表示“variant 展开/去掉 envelope”，原因是：

- `flat` 已有稳定语义：把嵌套 reflected object 的成员合并到父 object；
- variant alternative 可以是 scalar、array 或 object，“合并成员”对这些类型没有统一结果；
- 去掉 union envelope 的本质是“untagged union”，不是 object flatten；
- read 时会重新引入 alternative 歧义和破坏性试读，而普通 object flat 不涉及 discriminator；
- schema 中 `flat` 表示属性集合合并，用同名 tag 表示 `anyOf` 的 discriminator 策略会让工具无法解释。

### 7.4.3 使用 union 类别 tag，而不是 variant 专属 bool flag

已引入可扩展的 union 策略：

```cpp
enum class UnionEncoding {
    TaggedArray, // default: [index, value]
    Untagged,    // compatibility mode: bare active value
};

struct UnionTag {
    tag_value<UnionEncoding> encoding{};
};
```

使用形式例如：

```cpp
make_tags<UnionTag{.encoding = UnionEncoding::Untagged}>(field)
```

这不是为了 `std::variant` 单独增加一个零散 flag，而是定义 union-like 类型的共同策略入口。以后可在同一类别中增加 `TaggedObject`、稳定名称 discriminator 或显式 alternative ID，无需继续堆 bool。

### 7.4.4 `Untagged` 的约束

`Untagged` 只用于兼容已有文本格式或明确可区分的 alternatives，不作为默认值：

- 写入只输出 active payload；
- 读取必须保证候选试读不污染 Reader 状态；stream Reader 需要 checkpoint，AST Reader 也要 parse-then-commit；
- 如果多个 alternatives 都能接受同一输入，必须报告 ambiguous union，不能继续采用“第一个获胜”；
- Binary 中 `int/string` 可由 wire `ValueTag` 区分，但两个 reflected object 通常仍有歧义；
- schema 在 tagged 模式生成定长二元素 array，在 untagged 模式生成 `AnyOf`；调用方仍须把 alternatives
  可区分性作为协议约束；
- 不能与 `flat` 混为同一属性。

### 7.4.5 union tag 穿透规则

union wrapper 消费 `UnionTag`；默认 tagged 模式下 envelope node 接收 union/container 级 metadata，active payload 只接收过滤后的 payload metadata。untagged 模式没有 envelope，payload metadata 直接作用于 active alternative。

当前实现会从传给 active payload 的 tag 集合中移除 `UnionTag`，但保留 `BinaryTag` 等 payload metadata。
因此外层 `Untagged` 不会让内层 variant 也隐式变为 untagged。schema 入口也已支持 tag-aware 分派，保证
write/read/schema 对 `UnionEncoding` 使用同一判断。显式 untagged 读取会统计所有成功候选：零个时报 no-match，
多个时报 ambiguous，恰好一个时才重新解析并提交；Binary Reader 的 checkpoint 会恢复 cursor、node consumed
状态和 allocation budget，避免候选探测污染后续读取。

需要固定以下测试：

```cpp
TEST(Variant, AllBackendsDefaultToIndexValueArray);
TEST(Variant, UnionTagEnablesUntaggedCompatibilityMode);
TEST(Variant, UntaggedAmbiguousAlternativesAreRejected);
TEST(Variant, PayloadTagsAreSymmetricAndAppliedExactlyOnce);
TEST(Variant, ReorderedAlternativesBreakGoldenVectorByDesign);
```

长期应从“位置 index”升级为 schema 中显式、稳定的 alternative ID；在此之前，variant alternative 的顺序必须纳入协议兼容审查。

## 7.5 RPC dispatcher 的当前边界与公开调用上下文

```text
RpcServer
  ├── MethodRegistry              // 只读共享
  ├── ConnectionManager
  │     └── ConnectionContext
  │           ├── SessionId
  │           ├── PeerSession
  │           ├── recv loop
  │           ├── bounded task group
  │           └── active requests by RequestId
  └── GlobalLimits/Metrics
```

当前每个进入 handler 的请求拥有：

```text
RequestContext
  method
  request_id
  deadline
  cancellation_token
  peer
```

`RpcServer::bindMethodWithContext()` 把公共 `RpcRequestContext` 作为仅服务端可见的第一个参数；它不进入
RPC 方法的 wire 签名和参数序列化。wrapper 基类使用统一的 context 指针接口并声明 `usesContext()`：普通
handler 不构造 context；只有 context-aware handler 真正取得 active slot 后才惰性构造。`method` 使用调用期
只读 view，`peer` 引用 `addEndpoint(endpoint, RpcPeerInfo)` 中由服务提供方认证后给出的信息，不再为每个请求
复制 attributes map。context 不可复制、只在 handler 调用期有效，也不暴露 backend session、server 指针、
notification、全局负载或其他连接的数据。

客户端单次 `RpcCallOptions.deadline/timeout` 仍是本地契约：到期后删除 pending，并 best-effort 发送取消。
为了让服务端在入队时就知道调用者通常需要的时效，新增普通内建方法
`rpc.set_connection_timeout(timeout_ns)`，设置“本连接后续请求的默认相对 timeout”。`0` 清除；正值必须能用
`std::chrono::nanoseconds` 表示，且服务端配置 `request_timeout` 时必须严格小于该上限。设置成功后的新请求使用
`min(connection timeout, server request_timeout)`，覆盖 queue + handler；已经 admission 的请求不被追溯修改。
该连接状态在断开时删除，不形成任务历史。

这种连接级默认值不需要把客户端 deadline 偷渡进 Neko backend 私有 extension/TLV，也适用于其他 backend。
如果未来必须为每次调用传播不同 deadline，仍需先设计公开、backend-neutral、可版本协商的 request metadata
envelope，不能由通用 client 探测某个 backend 的私有编码能力。策略查询会如实返回
`deadline.propagation=connection_timeout_builtin_and_client_cancel`、当前连接值和可接受边界。

当前新增四个普通内建方法：

| 方法 | 返回范围 | 隐私与调度规则 |
| --- | --- | --- |
| `rpc.get_execution_policy` | 本连接适用的公开 frame、解压、per-connection 限制及 deadline 语义 | 不公开全局 active/queue 容量、全局指标或其他 peer 数据 |
| `rpc.set_connection_timeout` | 设置本连接未来请求的默认 timeout；`0` 清除，正值严格小于 server timeout | 设置调用本身也走普通 admission/timeout；不修改已经入队的请求 |
| `rpc.get_connection_status` | 调用者当前连接的 `active`、`queued`、`in_flight` 数量 | 排除查询调用自身，不跨连接 |
| `rpc.get_connection_tasks` | 调用者当前连接仍在 `queued`/`running` 的 response-bearing request id | 不保留已完成/失败/取消任务历史；连接清理时同步删除 |

这些查询与业务方法共用相同的 per-connection 上限、全局 admission、FIFO 排队、deadline 和 overload 反馈。
查询本身可能排队、超时或被拒绝，不提供 control-plane 特权或容量旁路。notification 没有可供调用者关联的
request id，因此不进入任务明细；它仍受相同执行容量约束。

本轮到此冻结范围：不增加任务历史、优先级、逐任务设置、后台特权查询或更多 context 元数据。当前四个内建
方法只回答连接当前状态和公开约束，避免把 RPC-015 扩张成任务管理系统。

---

# 8. 总任务 Checklist

> 下列列表只保留简要任务描述；标题链接到上面的详细分析。
> 状态更新于 2026-07-15：`[x]` 表示已实现并通过对应回归；`[ ]` 表示未完成、仅完成一部分或专项验证仍在进行。

## 8.0 本轮最终执行的修复方案

| 范围 | 最终实施方案 | 状态 |
| --- | --- | --- |
| Binary wire（SER-001/002/005/006/007/008/009/012/013） | 实施 Binary V2：使用 `4E 50 02` 魔数和显式 value tag；`null` 与字符串分离；variant 写入显式 index；整数使用规范 ULEB128/zigzag；浮点使用规范大端字节；对象可乱序读取并跳过未知字段；读取前限制输入、字符串、容器、嵌套深度和总分配；单根值必须完整消费。默认 tagged variant 不试读候选；仅显式 `Untagged` 兼容模式使用可恢复 checkpoint。 | 已完成 |
| Binary wire 规范注释 | `ValueTag` 实现前和第 7.1 节均给出 magic、tag、ULEB128/zigzag、容器/object、variant、raw fixed 的逐字节 grammar 与示例，并明确 wire tag 和 metadata tag 的区别。 | 已完成 |
| Binary 字段兼容与体积（SER-007） | 短字段名直接编码，较长字段名使用 FNV-1a 32-bit compact key；Writer/Reader 检测同一对象内的重复 key，Reader 为对象一次建立索引，避免每个反射字段重复扫描导致的 O(n²)。跨 schema 的 32-bit hash 碰撞和 rename 稳定性不由当前格式保证，限制见 7.2.5。 | 已完成（限制已记录） |
| 连续定长原始段 | 新增唯一显式标签 `BinaryTag{.raw_fixed_data = true}`，不提供旧名别名。它仅允许根级反射对象，且每个字段必须是带 `fixed_length` 的算术或枚举类型；不写魔数、类型、名称、数量和长度。后端仍保证 bool 仅为 0/1、枚举宽度和数值大端编码。通信头已迁移为精确 10 字节连续布局。 | 已完成 |
| `Object`/`IdObject` 边界 | `IdObject` 只用于 schema-known reflected object 的 compact key 能力；动态 object/map 保留可逆字段名。共同 child-value 接口和 field-key 差异已在 7.2 固化。后续是否改名为 `SchemaObject`、是否引入显式稳定 field ID 尚未实施。 | 当前行为已说明，演进项待定 |
| tag 读写对称（7.3） | 已增加统一 Reader tag-aware 适配层及 legacy fallback；scalar、fixed/raw string、null、sequence/map/tuple/pair、reflection 和 union 的当前 node 均有读写对称分派点。实体容器不向元素透传，optional/pointer 透明透传，`UnionTag` 只消费一层。 | 已完成 |
| union/variant 范式（7.4） | 通用 parser 已默认 `[index, value]`；`UnionTag{.encoding = UnionEncoding::Untagged}` 提供显式裸 payload 兼容模式，拒绝多候选歧义并对 Binary 探测做完整状态回滚；`UnionTag` 只消费一层，其他 payload tag 继续传播；schema 与 write/read 同步切换形态。`flat` 不复用为 union 展开。 | 已完成 |
| 通用反射解析（SER-003/004/010/011/014） | optional/flat 按实际输出字段计数；map、set、sequence 和反射对象先解析到临时值，全部成功后再提交；重复唯一键/元素明确报错；`to_json_value()` 改为返回可传播的 `Result`。 | 已完成 |
| 序列化根值语义（SER-015） | 所有 document backend 统一为单 root，第二次读写会明确失败；多值必须由用户包装为 tuple、array 或反射对象。本轮没有为 Binary 保留语义含糊的隐式 streaming API。 | 已完成 |
| RPC 隔离、并发与生命周期（RPC-001～006/009/010/014） | active request 和取消键改为 `(session, request-id)`；每批次使用独立 TaskGroup 和响应槽；client 使用单 receiver coroutine 与 pending-ID demux；server 的 recv loop 与 handler 执行解耦；endpoint 列表、发送、flush 和 shutdown 路径统一同步；用户 handler 异常转换为 InternalError。 | 已完成 |
| RPC 输入预算与协议校验（RPC-007/008/011/012/013） | 在分配前限制 frame、method/extensions、JSON message 和传输消息；限制解压输出和压缩比；method table 限制数量、ID 范围和稀疏度，并以临时表原子应用更新；`applyRemoteTable()` 只做一次 O(n) full-table 校验；JSON-RPC 返回正确的 ParseError/InvalidRequest，并严格验证 `result`/`error` 互斥性。 | 已完成 |
| deadline 与全局背压（RPC-015） | 新增公共 `RpcCallOptions`（absolute deadline、relative timeout、`stop_token`）；客户端超时/取消会删除 pending 并 best-effort 发送远程取消，late response 不会匹配后续调用。Server 增加跨连接的全局 active/queued 有界预算、每连接 in-flight 与 client pending 预算、FIFO admission 和 `Overloaded`/`DeadlineExceeded` 错误；server/connection timeout 覆盖排队与 handler 总时长；client/server 暴露 active、queued、completed、timed_out、canceled、rejected 指标。新增惰性公共 `RpcRequestContext`、connection-timeout 设置和 execution-policy/current-task builtins，不使用 backend 私有扩展传播客户端 deadline。 | 已完成；逐调用跨端 deadline metadata 作为显式后续协议设计 |
| 测试与工程化 | Binary 普通单测补齐独立预算、重复唯一值、golden vectors、固定种子随机值、随机错误 tag/截断前缀和异常合法值；RPC 普通单测补齐 deadline、timeout、取消、late response、跨连接背压、精确容量边界、指标、公开上下文、连接 timeout、当前任务查询及 close/partial-frame。聚焦回归分别为 Binary 30/30、RPC 40/40，JSON-RPC 端到端 11/11、协议 16/16、RPC traits 7/7；Debug 与 coverage 配置下的普通测试全集均为 25/25。Linux coverage 生成、ASan/UBSan/TSan 和 fuzz smoke workflow 已恢复，Actions/xmake 已固定版本与 SHA。 | 已完成并通过全量、sanitizer、fuzz 与 coverage 复验 |

## 8.1 序列化

- [x] [SER-001：为 null 增加独立 wire type，消除与字符串 `"null"` 的碰撞](#ser-001)
- [x] [SER-002：为 Binary variant 编码 discriminator，取消破坏性试读](#ser-002)
- [x] [SER-003：按实际输出字段计算 optional-like 对象成员数量](#ser-003)
- [x] [SER-004：修复 flat 展开后的父对象成员计数](#ser-004)
- [x] [SER-005：修复枚举名称/整数回退对 Binary Reader 的破坏性读取](#ser-005)
- [x] [SER-006：统一 float/double 的规范字节序和表示](#ser-006)
- [x] [SER-007：实现可跳过未知字段、顺序无关的 Binary V2 对象格式](#ser-007)
- [x] [SER-008：增加字符串、容器、深度和总分配解析限制](#ser-008)
- [x] [SER-009：默认启用单值完整消费检查，RPC payload 使用严格模式](#ser-009)
- [x] [SER-010：为重复 map key 和 set 元素增加明确失败策略](#ser-010)
- [x] [SER-011：解析到临时对象，成功后再提交目标值](#ser-011)
- [x] [SER-012：拒绝非法 bool、冗余 varint 和 signed negative zero](#ser-012)
- [x] [SER-013：统一验证 Binary InputValue，避免空 handle 解引用](#ser-013)
- [x] [SER-014：让 `to_json_value()` 返回可传播的错误结果](#ser-014)
- [x] [SER-015：统一为单 root API，多值通过 tuple/array/object 显式包装](#ser-015)
- [x] [记录 Binary V2 的完整逐字节 wire grammar 和 metadata tag 影响](#binary-wire-byte-layout)
- [x] [明确 `Object`/`IdObject` 的能力边界、收益和 hash 限制](#object-idobject-contract)
- [x] [补齐 container backend 的 tag-aware Reader hook，并按 scope 保证读写传播对称](#tag-propagation-contract)
- [x] [将所有 backend 的 union 默认统一为 `[index, value]`，新增 `UnionTag` untagged 兼容策略](#union-variant-contract)

## 8.2 RPC 与传输层

- [x] [RPC-001：按 session 隔离 active request、取消表和执行状态](#rpc-001)
- [x] [RPC-002：使用 batch-local TaskGroup 和安全的响应汇总方式](#rpc-002)
- [x] [RPC-003：修复 `close()` 后连接状态和 endpoint reset](#rpc-003)
- [x] [RPC-004：统一同步 client/server endpoint 生命周期](#rpc-004)
- [x] [RPC-005：实现 receiver loop 与 pending-request response demux](#rpc-005)
- [x] [RPC-006：让 server recv loop 与 handler 执行解耦](#rpc-006)
- [x] [RPC-007：在分配前限制 frame、method、payload 和 message 大小](#rpc-007)
- [x] [RPC-008：限制解压输出大小和最大压缩比](#rpc-008)
- [x] [RPC-009：实现可达且按 session 隔离的远程 Cancel 路径](#rpc-009)
- [x] [RPC-010：调整关闭顺序，确保 partial read 不阻塞 shutdown](#rpc-010)
- [x] [RPC-011：限制 method table 的 ID、数量和稀疏度；去掉 `applyRemoteTable()` 的重复 O(n) 校验](#rpc-011)
- [x] [RPC-012：补全 JSON-RPC parse error、空 batch 和非法 item 响应](#rpc-012)
- [x] [RPC-013：严格验证 JSON-RPC response 的 result/error envelope](#rpc-013)
- [x] [RPC-014：移除不真实的 `noexcept` 并建立异常边界](#rpc-014)
- [x] [RPC-015：增加 deadline、timeout、全局并发限制和背压](#rpc-015)

## 8.3 测试与 CI

- [x] [TEST-001：补齐 Binary serializer 关键回归矩阵](#test-001)
- [x] [TEST-002：增加 RPC 多连接、取消、关闭和 TSan 测试](#test-002)（普通 RPC 40/40 与 TSan RPC 40/40 均已通过）
- [x] [TEST-003：为 Binary、RPC frame、method table 和压缩增加 fuzz target](#test-003)（JSON 与统一 RPC/wire target 各完成 1000 次有界 smoke）
- [x] [CI-001：固定 xmake 和 GitHub Actions 版本并最小化权限](#ci-001)
- [ ] [CI-002：增加 ASan、UBSan、TSan、golden vector 和兼容性作业](#ci-002)（sanitizer、fuzz smoke、coverage 和 Binary golden 已建立且通过本地专项复验；仅旧版本兼容作业尚未新增）

## 8.4 2026-07-15 收尾进度与测试依据

本轮测试从公开 API、wire 数据和用户可观察反馈出发，不断言 dispatcher 的私有容器或调度步骤。普通自动测试使用同一套五点矩阵：

| 输入类别 | Binary serializer | RPC-015 |
| --- | --- | --- |
| 左边界 | 空容器允许 `max_container_elements=0`；预算为实际值减一时拒绝 | timeout 为 `0ns`/负值、active/queued/pending/in-flight 为 `0` 时立即拒绝；connection timeout 的 `0` 清除、`1ns` 为最小正值 |
| 中间随机值 | 固定种子生成普通整数、字符串、容器并 round-trip | 固定种子生成普通调用参数并经 runtime-name API 校验结果；connection timeout 使用正常中间值并实际终止长调用 |
| 右边界 | 输入、字符串、容器、对象、深度预算恰好等于数据时成功 | 1 active + 1 queued 恰好接纳；connection timeout 接受 server 上限减 `1ns`，等于上限即拒绝 |
| 随机错误值与反馈 | 随机非法 tag、所有截断前缀明确失败，原目标值保持不变 | 过去 deadline、超时、取消、过载分别返回稳定公开错误；随机越界 timeout 和 `uint64_t` 极值返回 `InvalidParams`，旧设置不被污染 |
| 非预期但合法值 | 整数极值、±inf、NaN、含 NUL/`0xff` 的字节串 | deadline 与 timeout 同时提供时较早者生效；无 server 上限时 24 小时 connection timeout 合法；迟到响应不污染下一调用 |

截至本次记录：

- `test_binary_serializer`：30/30 通过；
- `test_neko_rpc_backend`：40/40 通过；
- `test_jsonrpc`：11/11 通过；`test_jsonrpc_protocol`：16/16 通过；`test_func_traits`：7/7 通过；
- partial-frame 测试实际发现 body read 的不可取消等待，现已修复为 close 可在有界时间完成；
- 零 pending 容量测试实际发现刚启动 endpoint 后立即关闭的生命周期竞态，现通过“容量预检”和 server 先取消任务、再关闭 stream 的顺序修复；
- 当前任务查询测试覆盖：查询在普通全局队列中等待、只返回本连接 queued/running 项、跨连接不可见、调用全部收尾后结果为空；没有历史表和查询旁路；
- server deadline 测试覆盖“先排队、后执行”的总预算，排队消耗 deadline 后即使 handler 已开始也会按总时长返回 `DeadlineExceeded`；
- connection timeout 测试覆盖 `0`、`1ns`、中间值、server 上限减 `1ns`、等于/随机超过上限、`uint64_t` 极值、24 小时合法值、只影响未来请求和跨连接隔离；
- Debug 普通测试全集 25/25 通过；coverage 配置下同一全集 25/25 通过；
- 最终普通测试 coverage 为 lines 84.0%（6087/7243）、functions 70.8%（27956/39465）；RPC 核心文件分别为 dispatcher 252/265、client 132/144、server 180/190、options 24/25 行；
- ASan：Binary 30/30、RPC 40/40；UBSan：Binary 30/30、RPC 40/40；TSan：RPC 40/40。由于本地受 ptrace 约束，ASan/libFuzzer 运行使用 `detect_leaks=0`，地址与未定义行为插桩仍启用；
- libFuzzer：JSON proto 与统一 RPC/wire target 各完成 1000 次有界输入，无 crash 或 sanitizer 报告。

---

# 9. 首个修复里程碑建议

建议把第一个里程碑限定为以下可验证目标：

- Binary V2 可以正确 round-trip null、`"null"`、optional、variant、flat、enum 和 float；
- Binary V2 默认 strict、canonical，并带有限解析预算；
- RPC 两个连接使用相同 request ID 时完全隔离；
- Cancel frame 能停止正确请求；
- frame、解压和 method table 在分配前受限；
- client 支持至少多个并发 in-flight 请求；
- JSON-RPC 对非法 JSON、空 batch 和混合 batch 返回正确错误；
- 全部新增路径通过 ASan/UBSan，RPC 并发测试通过 TSan。

**2026-07-15 实施状态：**功能目标、普通 Debug/coverage 回归、ASan/UBSan/TSan 与 libFuzzer smoke 均已完成；仅旧版本数据兼容作业继续由 CI-002 跟踪。

完成该里程碑后，再考虑性能微优化、更多 serializer backend 和更复杂的 streaming RPC，会更稳妥。
