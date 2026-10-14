# ArgParser

`<nekoproto/argparser/argparser.hpp>` 中的 `argparser::parser<T>(argc, argv)` 从同一份 metadata 解析选项，普通选项结构体返回 `expected<T, std::error_code>`；纯子命令结构体的成功值为命令类型组成的 `std::variant`。检查错误后再读取结果；帮助请求可用 `formatHelp<T>()` 输出。完整 CLI 在 [examples/argparser.cpp](../examples/argparser.cpp)。

解析错误可与 `argparser::makeErrorCode(ArgParserError::...)` 比较；`error().message()` 提供稳定的错误类别，`argparser::lastError().message` 给出本线程最近一次解析的选项名和值等上下文。后者每次调用 `parser` 时重置，需在下一次解析前保存。`HelpRequested` 和 `VersionRequested` 也是返回的错误码，由调用者决定如何展示对应文本。

缺省值、配置文件、环境变量和命令行同时提供同一选项时，优先级依次为命令行、环境变量、导入的配置、缺省值。配置文件导入导出需要在 `ArgParserConfig::configIo` 中启用相应格式；导出的是解析后的值。独立的 `argparser::config_io` API 返回包含错误码、原因、格式和路径的 `ConfigIoError`，参见 `<nekoproto/argparser/config_io.hpp>`。xmake 单元测试覆盖这些优先级、缺失必需字段、约束、子命令和配置导入导出；新增用法以可运行的示例和公开头文件为准。
