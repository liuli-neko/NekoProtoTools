# Benchmarks

运行时 JSON 对比和编译时间工作负载统一放在本目录。C++20 运行时测试以同一业务模型比较 NekoProtoTools / yyjson、NekoProtoTools / simdjson、reflect-cpp / yyjson 和手写 RapidJSON；C++23 配置再加入 Glaze。依赖由 xmake 在启用选项时解析，具体版本记录在结果目录中。

## JSON 运行时对比

在仓库根目录运行：

```sh
xmake f -m release --stdcxx=20 --enable_benchmarks=y
xmake bench_json
```

要运行含 Glaze 的五方 C++23 对比，把配置命令中的 `--stdcxx=20` 改为 `--stdcxx=23`。第二行构建、交叉解析校验并运行对比。默认每个 case 测量 10000 次、重复 3 轮，结果在 `build/benchmarks/json/`。可用 task 参数指定规模和目录：

```sh
xmake bench_json -n 10000 -r 5 -o benchmarks/results/json-local
xmake test -g benchmark
```

输出文件包含可并排阅读的延迟、吞吐量、输出大小，以及原始记录和环境：

[C++20 本机样例结果](results/json-cpp20-2026-10-10/summary.md)保留了 4 个适配器、4 个 case、3 轮测量的完整原始记录及固定输入。

| 文件 | 内容 |
|---|---|
| `json.csv` | 主对比表：先列全部 serialize、再列全部 deserialize；各适配器的 `ns_per_op` 列相邻，`mb_per_second` 列相邻；值为各轮中位数 |
| `summary.md` | 按 serialize / deserialize 分组，各自给出延迟与吞吐量对照表，另列输出字节数 |
| `raw.csv` | 每轮原始测量，便于核对中位数 |
| `json_sizes.csv` | 每套实现各 case 的序列化输出字节数 |
| `environment.txt`、`compile-commands.txt` | 实际解析的依赖版本、运行参数和编译命令 |
| `fixtures/` | 固定输入与四套实现生成的 JSON |

所有启用的适配器使用 [`benchmark_models.hpp`](benchmark_models.hpp) 中相同的 C++ 类型与固定种子。计时前，每套适配器都要能解析其余适配器生成的 JSON；反序列化始终读取相同的 NekoProtoTools JSON。每个 case 计时前后验证往返结果，先做 100 次预热。序列化吞吐量按各自输出字节数计算，反序列化吞吐量按共同输入字节数计算。不同实现产生的 JSON 字节数可能不同，应结合 `json_sizes.csv` 解读吞吐量。

本机结果只说明该机器和已记录配置下的表现，不作为跨平台排名。复测时保持机器空闲，并保存输出目录。

## 编译时间工作负载

[compile_time/](compile_time/README.md) 保留反射和序列化编译工作负载，可在仓库根目录运行 `xmake bench_compile_time`。当前任务统一使用 C++20、`-O0 -c`、同一字段数和对应格式后端，输出每轮原始时间、编译参数与依赖路径；旧的 `RESULTS.md` 是方法对齐前的历史探索结果，不用于跨库排名，也不与上面的运行时 JSON 对比混用。

[C++20 本机探索结果](results/compile-time-cpp20-2026-10-10/summary.md)保留 3 轮测量的原始数据、准确编译命令和配置头；它只反映记录的机器、编译器及工作负载。
