# 2026-08-22 reflection compile-time results

环境：Windows x64、clang++ 22.1.8、C++23、`-O0`。Wall-clock 数据为同一机器、同一脚本连续
3 次独立编译的均值；before 使用仓库 `HEAD` (`01c1f3b`) 的归档头文件，after 使用本次重构后的
工作树。RapidJSON 来自本机 xmake package cache。

| workload | before (ms) | after (ms) | improvement |
| --- | ---: | ---: | ---: |
| reflection include | 1098.25 | 1092.28 | 0.5% |
| `forEach`, 4 fields | 1180.33 | 1204.71 | -2.1% |
| `forEach`, 16 fields | 1391.83 | 1298.95 | 6.7% |
| `forEach`, 32 fields | 1745.13 | 1417.76 | 18.8% |
| `forEach`, 64 fields | 3509.72 | 1854.25 | 47.2% |
| `forEach`, 64 fields, sparse tags | 3649.97 | 1899.97 | 47.9% |
| `forEach`, 64 fields, dense tags | 3711.45 | 1915.22 | 48.4% |
| binary write | 4485.50 | 2708.91 | 39.6% |
| binary read | 4037.76 | 2535.88 | 37.2% |
| schema | 2277.66 | 2208.44 | 3.0% |
| enum-heavy | 2196.69 | 1551.51 | 29.4% |
| JSON write | 4421.64 | 2671.20 | 39.6% |
| JSON read | 4109.89 | 2564.22 | 37.6% |

Clang `-ftime-trace` 的单次可见事件（事件记录受 Clang duration granularity 影响）：

| trace | metric | before | after |
| --- | --- | ---: | ---: |
| 64-field `forEach` | `InstantiateFunction` events | 1140 | 386 |
| 64-field `forEach` | generated indexed accessor events | 389 | 64 |
| JSON write | `InstantiateFunction` events | 1542 | 1018 |
| JSON read | `InstantiateFunction` events | 1321 | 949 |
| schema | `InstantiateFunction` events | 893 | 769 |
| enum-heavy | total `EvaluateAsConstantExpr` time | 91.20 ms | 15.32 ms |
| enum-heavy | total `EvaluateAsRValue` time | 419.60 ms | 128.36 ms |

在该 `-O0` compile-only workload 中，64-field `forEach` object 从 11,032,813 bytes 降到
1,144,364 bytes；JSON write/read object 分别下降约 81.0%/85.9%。这是生成的重复 accessor
函数消失后的直接结果，不是 runtime data layout 变化。

验证过的编译器：MSVC 19.44（完整测试组）、Clang 22.1.8（benchmark/trace）、GCC 16.2
（64-field dense-tag workload）。
