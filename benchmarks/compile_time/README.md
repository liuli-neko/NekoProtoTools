# Compile-Time Benchmark Suite

These workloads are exploratory. The task defaults to C++20 (select `--std=20/23/26`), compiling both implementations with `-O0 -c`, the same selected field count, and matching JSON (yyjson), YAML (yaml-cpp), TOML (toml++), and XML (pugixml) backends. The older checked-in [RESULTS.md](RESULTS.md) predates this alignment and is historical only. Validate feature and output equivalence before publishing a ranking.

This benchmark suite measures translation-unit compile time and object file size for reflection and serialization workloads, comparing **NekoProto** with **reflect-cpp** across multiple formats and field counts.

## Features

- **Direct Head-to-Head Comparison with reflect-cpp**:
  - Core header include overhead (`#include <nekoproto/serialization/reflection.hpp>` vs `#include <rfl.hpp>`)
  - Field-count scaling: JSON Write & Read at **4, 16, 32, and 64 fields**
  - Full-featured formats at **64 fields**: **JSON, YAML, TOML, XML**
  - Compares compile-time wall-clock latency (ms) and generated object symbol size (`.o` bytes)
- **NekoProto-Specific Workloads**:
  - Fast binary wire format Read & Write (64 fields)
  - `Reflect<T>::forEach` member iteration scaling (4, 16, 32, 64 fields)
  - Tag density scaling (sparse vs dense 4 tags/field)
  - Dynamic runtime schema reflection generation
  - `constexpr` enum mapping table evaluation (10 enums)
- **Zero Build Pollution**:
  - Integrated directly into `xmake` as a manual on-demand task (`xmake bench_compile_time`)
  - Never run automatically during standard builds (`xmake build` or `xmake test`)
  - Resolves package include paths (`yyjson`, `yaml-cpp`, `toml++`, `pugixml`, `reflect-cpp`, `zeus_expected`, `fmt`) and records the paths used

---

## Running the Benchmark

### 1. Basic Execution (Manual Trigger via xmake)

```bash
# Configure once, then run with the auto-selected compiler and 3 iterations
xmake f -m release --stdcxx=20
xmake bench_compile_time

# Run and automatically save the results table to RESULTS.md
xmake bench_compile_time --save

# Run with custom iterations (e.g. 5 repeats for greater accuracy)
xmake bench_compile_time -r 5
```

### 2. Selective Filtering

```bash
# Benchmark only JSON workloads
xmake bench_compile_time -f json

# Benchmark only 64-field workloads
xmake bench_compile_time -f 64

# Benchmark only YAML and TOML
xmake bench_compile_time -f yaml
xmake bench_compile_time -f toml
```

### 3. Custom Compiler & Clang `-ftime-trace`

```bash
# Specify explicit compiler binary
xmake bench_compile_time -c clang++
xmake bench_compile_time -c /usr/bin/g++

# Enable Clang time-trace profiling
xmake bench_compile_time --trace -o build/compile_time/trace_run
```

### 4. Output Artifacts

Install missing dependencies before running comparative cases, for example with `xrepo install reflect-cpp`. The task does not install packages during measurement. It uses an isolated copy of `config.h` so optional backends from the current xmake configuration do not affect unrelated workloads. A selected workload that fails compilation makes the task fail.

Running the benchmark generates:
- **Terminal Report**: High-resolution ASCII comparison table with colored delta indicators and format totals.
- **CSV Data**: `build/compile_time/<label>/timings.csv` containing detailed `min`, `max`, `avg`, and `size_bytes`.
- **Raw data**: `raw.csv` with every compile duration, `commands.txt` with the exact compiler argv, and `environment.txt` with the source commit, compiler, flags, and resolved package paths.
- **Markdown Report**: `benchmarks/compile_time/RESULTS.md` (when triggered with `--save`).

## C++26 backend and feature comparisons

Reuse this task with `--std=26 --reflection-backend=legacy/native/auto` and an
optional `--include-root=/path/to/include` snapshot. GCC native/auto comparisons
add `-freflection`. Run the existing baseline once with `-r 1`; it is not a new
test framework.
