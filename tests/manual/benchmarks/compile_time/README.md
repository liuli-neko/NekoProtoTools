# Compile-Time Benchmark Suite

This benchmark suite measures reflection and serialization compile-time performance, template expansion overhead, and object file symbol footprint, directly comparing **NekoProto** against **reflect-cpp** across multiple serialization formats and field-count dimensions.

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
  - Auto-discovers and resolves package include paths (`rapidjson`, `simdjson`, `libfyaml`, `yaml-cpp`, `toml++`, `pugixml`, `reflect-cpp`, `fmt`)

---

## Running the Benchmark

### 1. Basic Execution (Manual Trigger via xmake)

```bash
# Run with default settings (auto compiler, 3 iterations per workload)
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

Running the benchmark generates:
- **Terminal Report**: High-resolution ASCII comparison table with colored delta indicators and format totals.
- **CSV Data**: `build/compile_time/<label>/timings.csv` containing detailed `min`, `max`, `avg`, and `size_bytes`.
- **Markdown Report**: `tests/manual/benchmarks/compile_time/RESULTS.md` (when triggered with `--save`).
