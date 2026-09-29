# Benchmarks

运行时 JSON 对比和编译时间工作负载统一放在本目录。运行时测试比较 NekoProtoTools / RapidJSON、reflect-cpp / yyjson、Glaze 的同一业务模型；依赖由 xmake 在启用选项时解析，未固定版本。

## JSON 运行时对比

在仓库根目录运行：

```sh
xmake f -m release --enable_benchmarks=y
xmake run bench_json
```

第一行启用三方依赖与 benchmark targets；第二行构建、交叉解析校验并运行对比。默认每个 case 测量 10000 次、重复 3 轮，结果在 `build/benchmarks/json/`。可用 task 参数指定规模和目录：

```sh
xmake bench_json -n 10000 -r 5 -o benchmarks/results/local-2026-09-29
xmake test -g benchmark
```

[本机样例结果](results/local-2026-09-29/summary.md)包含可并排阅读的延迟、吞吐量、输出大小，以及原始记录和环境。输出文件：

| 文件 | 内容 |
|---|---|
| `json.csv` | 主对比表：先列全部 serialize、再列全部 deserialize；三个 `ns_per_op` 列相邻，三个 `mb_per_second` 列相邻；值为各轮中位数 |
| `summary.md` | 按 serialize / deserialize 分组，各自给出延迟与吞吐量对照表，另列输出字节数 |
| `raw.csv` | 每轮原始测量，便于核对中位数 |
| `json_sizes.csv` | 三套实现各 case 的序列化输出字节数 |
| `environment.txt`、`compile-commands.txt` | 实际解析的依赖版本、运行参数和编译命令 |
| `fixtures/` | 固定输入与三套实现生成的 JSON |

三套适配器使用 [`benchmark_models.hpp`](benchmark_models.hpp) 中相同的 C++ 类型与固定种子。计时前，三个适配器都要能解析彼此生成的 JSON；反序列化始终读取相同的 NekoProtoTools JSON。每个 case 计时前后验证往返结果，先做 100 次预热。序列化吞吐量按各自输出字节数计算，反序列化吞吐量按共同输入字节数计算。不同实现产生的 JSON 字节数可能不同，应结合 `json_sizes.csv` 解读吞吐量。

本机结果只说明该机器和已记录配置下的表现，不作为跨平台排名。复测时保持机器空闲，并保存输出目录。

## 编译时间工作负载

[compile_time/](compile_time/README.md) 保留现有反射和序列化编译工作负载，可在仓库根目录运行 `xmake bench_compile_time`。现有跨库编译案例的标准、编译参数和依赖发现方式尚未统一，因此其历史结果仅供探索，不与上面的运行时 JSON 对比混用。
