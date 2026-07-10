# ArgParser 加固计划

本文档定义 ArgParser 下一阶段的行为约定和实施顺序。目标不是把所有误用都变成昂贵的编译期元编程，而是在 schema 构建期消除无歧义的错误，并把其余语义写成稳定、可测试的契约。

## 已确定的规则

### 错误诊断

`std::error_code` 只表达稳定的错误枚举值；它的 `message()` 不再承载一次解析调用的上下文。解析期间产生的上下文通过公开的 `last_error()` 获取，返回拥有 `code` 和 `message` 的值对象。调用者若要跨后续解析保存诊断，保存该返回值即可。

### 结果所有权

ArgParser 返回的对象必须独立于本次解析内部的临时存储。因此不支持 `std::string_view`，也不支持其 `std::optional` 或 `std::vector` 包装作为 option 字段。命令行文本、环境变量、隐式值和配置文件输入都可以来自短生命周期缓冲区；需要文本字段时使用 `std::string`。

### 名称与嵌套路径

- 默认字段名和 `arg_long_name` 都是当前嵌套层的**叶子名称**；父级前缀会自动拼接。
- `arg_absolute_name<"a.b">` 使用完整的绝对 CLI 名称，不添加父级前缀。路径分隔符使用 `ArgParserConfig::nestedSeparator`，默认是 `.`。
- 长名称和长别名使用 ASCII 字母、数字、`-`、`_`；不能为空、包含空白、`=`，或产生空路径段。短名称使用一个可见 ASCII 字符，但不能是 `-`、`=` 或空白。
- 所有用户 long name、short name 和 alias 必须唯一；冲突返回 `InvalidDefinition`，不得依赖声明顺序“先到先得”。用户定义的 `-h`、`--help`、`-V`、`--version` 优先于同名内建行为；内建 help/version 只处理未被用户 schema 命中的 token。

### `requires` / `conflicts` 引用

关系 tag 中的名称解析规则如下（下例分隔符为 `.`）：

| 写法 | 含义 | 在 `auth.login` 上的结果 |
|---|---|---|
| `"token"` | 当前层级的同级字段 | `auth.token` |
| `"database.host"` | 兼容现有行为的根绝对路径 | `database.host` |
| `"/database.host"` | 显式根绝对路径，推荐新代码使用 | `database.host` |
| `"./tls.cert"` | 相对当前层级 | `auth.tls.cert` |
| `"../token"` | 相对父层级，可连续使用 `../` | `token` |
| `"-v"` / `"--verbose"` | 显式短/长 option 拼写 | 对应的 option |

解析不到、引用自身或引用不唯一均是 `InvalidDefinition`。这项检查在任何参数 materialize 之前完成，因此不会因引用 option 未出现而被漏掉；校验成功后，schema 会将每条关系保存为目标 option 的索引，运行期和 help 均不再重新解析原始字符串。

关系始终指向 option 的最终公开 CLI 名称，而不是 C++ 字段所在位置。对于 `arg_absolute_name<"listen-port">`，跨层引用推荐显式写为 `"/listen-port"`；`../listen-port` 只有在从当前 scope 回退后恰好到达其有效 scope 时才成立。help 不显示 `./`、`../` 等内部写法，而显示解析后的规范标签，例如 `--auth.login (requires: --auth.token)`。

### 值、重复和关系激活

- 只有 `std::vector<T>` 能 repeat；标量使用 `ArgTags{.repeatable = true}` 是编译期错误。
- 非 repeatable option（包括 flag）在一次命令行中出现第二次时返回 `InvalidValue`，不再静默以最后一个值覆盖前值。
- repeatable positional 必须是最后一个 positional。
- range 必须同时声明 `range_min` 与 `range_max`，并使用半开区间 `[min, max)`；单边界 range 不再被隐式解释为以 `0` 为另一边界。
- `required` 判断“是否由某个来源提供”；`requires` / `conflicts` 判断“是否激活”。对 bool，`false` 不激活关系，因此 `--json=false --yaml` 合法；`--json --yaml` 不合法。其他标量按是否提供判断，optional 按是否有值判断，vector 按是否非空判断。
- 值优先级固定为：CLI > 环境变量 > 配置导入 > `arg_default` > 结构体初始化值。`arg_default` 会满足 `required`。

### Help 与 usage

自动 usage 固定以 `[options]` 开始，并按声明顺序追加可见 positional：必填 positional 记作 `<NAME>`，可选 positional 记作 `[<NAME>]`，repeatable positional 追加 `...`。help 中 named options 保持在 `Options:`（和分组）内，positionals 单独列在 `Arguments:`，避免两类参数交织。若设置 `ArgParserConfig::usage`，该字符串由调用者完全控制，不再自动追加 positionals。

### 配置文件

配置导入导出复用统一 serializer。该 serializer 已要求所有非 optional、非 skippable 的反射字段在输入中出现；缺失字段会使导入失败。ArgParser 依赖这个契约，并补充回归测试。配置中的 `false` bool 是“已提供但未激活”，可满足 `required`，但不会触发关系约束。

### 命令与未知参数

子命令名称目前只支持主名称（反射名或 `arg_long_name`）；`arg_aliases` 不适用于命令字段。命令根部与普通 options 对 `allowUnknown` 使用同一规则：未知 token 可以被跳过，但不会猜测或吞掉其后的值。需要透传未知参数的调用方应使用 `--` 或未来的“剩余参数”API。

## 实施项

1. 将错误详情迁移为公开的 `last_error()` 值 API，恢复 `error_code.message()` 的稳定性。
2. 在 schema 构建期加入低成本静态约束：借用文本字段、标量 repeatable、separator 非 vector。
3. 加入运行期 schema 定义校验：名称冲突、保留名称、位置参数顺序、关系引用。
4. 增加 `arg_absolute_name`，并实现上述引用路径解析。
5. 统一重复 option、短 option cluster、命令根未知 option 与 bool 关系激活语义。
6. 扩充 help、公开头文件注释、中英文 README，并将 README 代码段纳入回归测试。
7. 增加正反向单测：嵌套引用、绝对名称、重复标量、`false` flag 关系、配置缺字段、诊断稳定性。

## 非目标

- 不在这一轮引入任意容器、任意自定义文本类型或宽字符 argv 支持。
- 不把所有 schema 错误都变成模板级重复字符串计算；跨字段冲突和路径解析保持为一次 schema 构建时的线性/小规模检查。
- 不在这一轮支持子命令别名和“带值的未知参数透传”；文档明确当前边界。
