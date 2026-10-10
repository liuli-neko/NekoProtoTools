# C++20 compile-time benchmark: local exploratory run

Source commit: `3a927c8e15dbce17eab10f0aebda2da438d94e31`. The working tree also contained an unrelated modification to `tests/unit/proto/test_random_big_data.cpp`. Run on Linux x86_64 with Debian Clang 21.1.8, `-std=c++20 -O0 -c`, three serial repetitions per workload.

```sh
xmake bench_compile_time -r 3 -o benchmarks/results/compile-time-cpp20-2026-10-10
```

All 40 selected translation units compiled successfully in each repetition: 120 raw timings, 15 matched comparisons, and 10 NekoProtoTools-only workloads. The matched comparison uses the same field count and corresponding JSON, YAML, TOML, or XML backend on both sides. Package versions and paths are recorded in [environment.txt](environment.txt), exact compiler arguments in [commands.txt](commands.txt), all repetitions in [raw.csv](raw.csv), and aggregates plus object sizes in [timings.csv](timings.csv). The copied [config.h](config/nekoproto/global/config.h) is needed to replay the NekoProtoTools commands.

The matched translation units sum to 48.98 seconds for NekoProtoTools and 64.63 seconds for reflect-cpp when using the mean of three runs per unit. Individual results and object sizes vary by workload. These figures describe this compiler and these source files; output and feature equivalence have not been established for every format, so they are not a general library ranking. The historical [RESULTS.md](../../compile_time/RESULTS.md) used different settings and should not be compared numerically with this run.
