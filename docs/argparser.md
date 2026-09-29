# ArgParser

`<nekoproto/argparser/argparser.hpp>` 中的 `argparser::parser<T>(argc, argv)` 从同一份 metadata 解析选项，普通选项结构体返回 `expected<T, std::error_code>`；纯子命令结构体的成功值为命令类型组成的 `std::variant`。检查错误后再读取结果；帮助请求可用 `formatHelp<T>()` 输出。完整 CLI 在 [examples/argparser.cpp](../examples/argparser.cpp)。

xmake 单元测试覆盖选项、默认值、约束、子命令和配置导入导出；新增用法以可运行的示例和公开头文件为准。
