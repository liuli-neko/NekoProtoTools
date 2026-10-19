# C++26 反射与模块使用

本次适配的核心是通过原生反射发现字段、提取注解和枚举项，上层仍使用 `Reflect<T>`、`FieldDescriptor` 和现有序列化、CLI、RPC 接口。C++20/23 继续支持。原生能力不以当前编译器的性能收益为启用条件。

## 选择后端

| 配置 | 对象字段来源 | 自动枚举发现 |
| --- | --- | --- |
| `legacy`（首次发布默认） | 自定义 metadata，否则原有聚合展开 | 原有扫描表 |
| `auto` | 自定义 metadata，否则在能力和类型受支持时使用原生反射，最后原有展开 | 原有扫描表 |
| `native` | 自定义 metadata，否则原生反射；不支持的类型维持既有路径 | 原生枚举声明 |

xmake 的自动后端拼写为 `--reflection_backend=automatic`，自动语法开关也使用 `automatic`；它将 `auto` 作为构建工具保留值。CMake 和头文件仍使用 `auto`，选择规则相同。

每个类型整体选择一种来源：**自定义 metadata > 原生反射及注解 > 自动推导**。字段、顺序、名称和 tags 不跨来源合并或补全。已选择来源的错误直接报告。默认假设业务类型只采用一种声明方式。

```sh
# GCC 16.2：头文件模式
xmake f --stdcxx=26 --reflection_backend=native

cmake -S . -B build -DCMAKE_CXX_STANDARD=26 \
  -DNEKO_PROTO_REFLECTION_BACKEND=native
```

直接调用 GCC 时使用 `-std=c++26 -freflection -DNEKO_PROTO_REFLECTION_BACKEND=1`。`0/1/2` 分别对应 `legacy/native/auto`。CMake/xmake 先测试标准模式，必要时编译测试 `-freflection`，通过标准宏和实际表达式确认能力；不根据编译器版本猜测。

原生语言、库能力由 `__cpp_impl_reflection` 和 `__cpp_lib_reflection >= 202506L` 判断。注解属于这一组标准反射能力；GCC 的对应支持记录见 [官方特性表](https://gcc.gnu.org/projects/cxx-status.html)。无可靠判断依据的扩展保持关闭。

## 按类型迁移

```cpp
#include <nekoproto/reflect.hpp>

struct User {
    [[=nekoproto::rename_tag<"user_id">]] int id;
    [[=nekoproto::ParserTag{.skippable = false}]] std::string name;
    User(int i, std::string n) : id(i), name(std::move(n)) {}
};

static_assert(nekoproto::Reflect<User>::value_count == 2);
static_assert(nekoproto::Reflect<User>::indexOf<&User::id>() == 0);
static_assert(nekoproto::FieldDescriptor<User, 0>::name == "user_id");
```

迁移一个类型时移除该类型的 `Meta<T>` / `T::Neko` 声明，将所需 tags 放到字段注解中。其余类型继续使用原声明方式。若保留自定义 metadata，其字段集合和 tags 全部优先，原生注解不会补充缺省信息。

首轮对象范围：完整的、无基类、非 union、无匿名 union 成员、无位域且所有直接数据成员满足 public 访问契约的类。支持非聚合和非默认构造类、空类、const 对象、引用成员和数组成员。`value_types/FieldType` 保持既有 decay 语义，字段访问仍保留真实数组引用及 cv 限定。不静默过滤不可访问或不支持的字段；这些类型维持原路径，必要时使用显式 metadata。

注解值直接进入现有 tags 容器，不要求标记成员、基类或适配器，不做类型转换。内置选项、自定义结构值、数字和编译期字符串均沿用现有 tag 表示约束；嵌套 `TagList`、`NoTags`、显式 `false` / `0` 和类型校验保持原有语义。不同消费者只查询自己认识的 tag，其他值仍可由用户反射功能读取。

```cpp
struct AuditLabel { int category; };
struct Item {
    [[=AuditLabel{7}, =42, =nekoproto::ParserTag{.skippable = false}]] int value;
};
constexpr auto tags = nekoproto::Reflect<Item>::tagOf<&Item::value>();
static_assert(tags.get<0>().category == 7);
static_assert(tags.get<1>() == 42);
```

用户需要 `ParserTag` 时直接提供它。库不把其他类型转成 `ParserTag`，同类型的显式重复值继续遵循现有 tag 查询规则。

原生字段发现不改变公共类名、RPC 方法命名算法或方法 ID。已有专用表示的 `NameValuePair`、`BinaryData`、tuple/pair 等仍由对应序列化规则处理。格式名称由原有 tag 查询和字段计划决定。

## 原生枚举

`native` 显式启用原生枚举表；`auto` 保持原有枚举发现行为。原生表包含全部声明项，支持稀疏、负值、大值、别名和枚举项注解。顺序是声明顺序；同值转名称选择首个声明项，每个声明名称都可转回对应值。

显式 `Meta<Enum>::value = Enumerate(...)` 始终优先。迁移原生枚举后，过去扫描范围外的已声明值也有名称，文本序列化会相应使用名称；需要固定已有线格式时保留显式 `Enumerate`。

## 独立语法开关

| 能力 | 头文件宏 | CMake | xmake | 默认 |
| --- | --- | --- | --- | --- |
| 绑定包 | `NEKO_USE_STRUCTURED_BINDING_PACKS` | `NEKO_PROTO_BINDING_PACKS=auto/on/off` | `binding_packs=automatic/on/off` | 标准宏支持时启用 |
| tag 包索引 | `NEKO_USE_PACK_INDEXING` | `NEKO_PROTO_PACK_INDEXING=auto/on/off` | `pack_indexing=automatic/on/off` | 标准宏支持时启用 |
| 展开语句遍历 | `NEKO_USE_EXPANSION_STATEMENTS` | `NEKO_PROTO_EXPANSION_STATEMENTS=ON/OFF` | `expansion_statements=y/n` | 关闭 |

绑定包使聚合绑定不再依赖生成的 128 字段分派；关闭时继续使用原生成头。包索引直接选取 tag。展开语句只替换短路遍历阶段，保持对象绑定一次和返回值语义。它们均独立于反射来源选择。显式启用不可用特性会产生诊断。

## 共用头文件的模块

```sh
cmake -S . -B build -G Ninja -DCMAKE_CXX_STANDARD=26 \
  -DNEKO_PROTO_REFLECTION_BACKEND=native -DNEKO_PROTO_BUILD_MODULES=ON
cmake --build build

xmake f --stdcxx=26 --reflection_backend=native --enable_modules=y
xmake build
```

模块构建统一开启，为所有已启用组件提供入口；组件功能仍由原有选项选择。头文件继续安装并可独立使用。序列化模块包含 binary、文本格式化以及配置中已启用的格式后端。

| 模块名 | CMake 目标 | xmake 目标 | 启用条件 |
| --- | --- | --- | --- |
| `nekoproto.reflect` | `NekoProto::ReflectionModule` | `NekoReflectionModule` | 始终 |
| `nekoproto.serializer` | `NekoProto::SerializerModule` | `NekoSerializerModule` | 始终 |
| `nekoproto.argparser` | `NekoProto::ArgParserModule` | `NekoArgParserModule` | 始终 |
| `nekoproto.proto` | `NekoProto::ProtocolModule` | `NekoProtocolModule` | Protocol |
| `nekoproto.transport` | `NekoProto::TransportModule` | `NekoTransportModule` | Communication 或 JSON-RPC |
| `nekoproto.rpc` | `NekoProto::RpcModule` | `NekoRpcModule` | JSON-RPC / Neko RPC |
| `nekoproto.jsonrpc` | `NekoProto::JsonRpcModule` | `NekoJsonRpcModule` | JSON-RPC / Neko RPC |
| `nekoproto.communication` | `NekoProto::CommunicationModule` | `NekoCommunicationModule` | Communication |

下游链接所用的模块目标，构建工具负责其依赖；例如 `target_link_libraries(app PRIVATE NekoProto::ArgParserModule)`。入口通过 `export import` 复用组件依赖，各声明和导出标记保留在原头文件中，没有第二份 API 清单。安装包包含模块源，由消费工具链生成 BMI。

头文件模式和模块模式共用 `reflect.hpp`、`serialization.hpp`、`argparser/argparser.hpp` 等功能汇总入口。模块接口用 `NEKO_BUILDING_MODULE=NEKO_MODULE_组件名` 标记当前编译的组件；汇总入口只展开当前组件，其他组件的 API 由 `export import` 提供。它是内部编译标记，不是按组件启用模块化的开关。

新增普通头文件时，直接包含 `global/macros.hpp`、同组件文件和所需功能的汇总头，无须编写模块条件。例如 ArgParser 的普通头文件使用：

```cpp
#include "nekoproto/global/macros.hpp"
#include "nekoproto/argparser/detail/config_io_registry.hpp"
#include "nekoproto/serialization.hpp"
```

新增跨组件依赖时，包含对应功能汇总头，并在 `.cppm` 中补充相应的 `export import`。`global/macros.hpp` 汇总配置、导出、可见性和日志宏；模块专用的预加载与导出设置集中在 `modules/detail/*module_prelude.hpp` 和 `modules/detail/module_exports.hpp`，不维护另一套 API 汇总清单。

CMake 模块路径要求 3.28+、Ninja 和支持扫描的编译器；验证工具链为 CMake 3.31.6 / GCC 16.2。头文件构建仍保持 CMake 3.23 最低要求。模块中的第三方依赖按原功能配置解析。

```cpp
import nekoproto.reflect;
struct Point { int x; int y; };
// Meta 特化、注解值和 Reflect 使用方式与头文件相同。
```

支持一个 TU include、另一个 TU import，且共享类型、metadata 特化、TagList tuple/ADL 协议一致，纯 import 无须先包含 `<tuple>`。模块的后端、语法和日志配置在构建时固定，自动选择也导出为具体 USE 值；各 TU 使用同一目标、编译器、语言模式和 ABI 配置。改变这些条件时重建模块及库。

C++ 宏不随 import 导出。需要 tag property 或 `NEKO_SERIALIZER` 宏时，在 import 后包含仅含宏的 `<nekoproto/reflection/tag_macros.hpp>` 或 `<nekoproto/serialization/macros.hpp>`。`NEKO_DECLARE_PROTOCOL` 对应 `<nekoproto/proto/macros.hpp>`。

依赖统一使用 Ilias 0.5.1。模块模式同步开启其模块支持；Transport 导入并重新导出 `ilias`，RPC、JSON-RPC 和 Communication 通过依赖链共用它。导入后可以直接使用 Ilias API，所需宏由 `<ilias/macros.hpp>` 提供。CMake 使用上游 `ilias.cppm`，xmake 使用依赖包发布的同一入口。

0.4.1 移除了三个纯转发头：tags 使用 `<nekoproto/reflection/tags.hpp>`，编译期字符串使用 `<nekoproto/global/string_literal.hpp>`，文本格式化使用 `<nekoproto/serialization/to_string.hpp>`。需要全部已启用格式时包含 `<nekoproto/serialization.hpp>`。

正式回归在 `tests/cpp26` 和 `tests/modules`，由 CTest / `xmake test` 执行。安装消费工程在 `tests/cmake/module-consumer` 和 `tests/xmake/module-consumer`。CI 固定 GCC 镜像，并覆盖 C++20/23/26 legacy 与 C++26 native。编译成本对照复用现有 [编译基准](../benchmarks/compile_time/README.md)，不为原生能力设置性能准入门槛。
