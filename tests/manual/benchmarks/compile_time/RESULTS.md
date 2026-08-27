# Reflection & Serialization Compile-Time Benchmark Results

Environment: linux (x86_64), Compiler: `Debian clang version 21.1.8 (3~bpo13+2)`, Optimization: `-O0`, Repeats: 3

## Head-to-Head Comparison: NekoProto vs reflect-cpp

| Workload | Description | NekoProto (ms) | reflect-cpp (ms) | Speedup (%) | Speedup (x) | Neko Obj | reflect-cpp Obj |
|:---|:---|---:|---:|---:|---:|---:|---:|
| `include_cost` | Core header include overhead | 1008.0 | 1450.7 | +30.5% | 1.44x | 696 B | 744 B |
| `json_write_4` | JSON write (4 fields) | 1592.3 | 1623.3 | +1.9% | 1.02x | 443.0 KB | 127.5 KB |
| `json_read_4` | JSON read (4 fields) | 1595.0 | 1640.3 | +2.8% | 1.03x | 417.7 KB | 296.0 KB |
| `json_write_16` | JSON write (16 fields) | 1708.3 | 1821.7 | +6.2% | 1.07x | 640.0 KB | 380.5 KB |
| `json_read_16` | JSON read (16 fields) | 1746.0 | 1919.7 | +9.0% | 1.10x | 602.5 KB | 811.8 KB |
| `json_write_32` | JSON write (32 fields) | 1886.7 | 2062.3 | +8.5% | 1.09x | 815.4 KB | 747.3 KB |
| `json_read_32` | JSON read (32 fields) | 1855.7 | 2348.0 | +21.0% | 1.27x | 741.3 KB | 1.45 MB |
| `json_write_64` | JSON write (64 fields) | 2515.3 | 3425.7 | +26.6% | 1.36x | 1.23 MB | 1.86 MB |
| `json_read_64` | JSON read (64 fields) | 2613.3 | 4114.3 | +36.5% | 1.57x | 1.08 MB | 3.53 MB |
| `yaml_write_64` | YAML write (64 fields) | 2530.3 | 3380.3 | +25.1% | 1.34x | 1.17 MB | 1.88 MB |
| `yaml_read_64` | YAML read (64 fields) | 2340.7 | 4151.7 | +43.6% | 1.77x | 963.8 KB | 3.70 MB |
| `toml_write_64` | TOML write (64 fields) | 3002.7 | 3769.7 | +20.3% | 1.26x | 1.49 MB | 2.16 MB |
| `toml_read_64` | TOML read (64 fields) | 2955.3 | 4443.7 | +33.5% | 1.50x | 1.68 MB | 4.18 MB |
| `xml_write_64` | XML write (64 fields) | 2389.0 | 3118.7 | +23.4% | 1.31x | 1.16 MB | 1.89 MB |
| `xml_read_64` | XML read (64 fields) | 2387.7 | 3929.0 | +39.2% | 1.65x | 1006.1 KB | 3.70 MB |
| **TOTAL (ALL FORMATS)** | Cumulative Benchmark Sum | **32126.3** | **43199.0** | **+25.6%** | **1.34x** | **13.31 MB** | **26.66 MB** |

## NekoProto Feature & Scaling Workloads

| Workload | Description | Compile Time (ms) | Object Size |
|:---|:---|---:|---:|
| `binary_write_64` | Neko binary write (64 fields) | 2548.7 | 1.19 MB |
| `binary_read_64` | Neko binary read (64 fields) | 2557.3 | 1.24 MB |
| `foreach_4` | Reflect::forEach (4 fields) | 1136.7 | 53.3 KB |
| `foreach_16` | Reflect::forEach (16 fields) | 1291.7 | 181.6 KB |
| `foreach_32` | Reflect::forEach (32 fields) | 1495.3 | 383.1 KB |
| `foreach_64` | Reflect::forEach (64 fields) | 2116.7 | 919.8 KB |
| `foreach_64_sparse` | Reflect::forEach (64 fields, 1 tag/field) | 2111.0 | 953.8 KB |
| `foreach_64_dense` | Reflect::forEach (64 fields, 4 tags/field) | 2146.3 | 976.2 KB |
| `schema_64` | Runtime schema generation (64 fields) | 2072.0 | 681.0 KB |
| `enum_heavy` | Constexpr enum tables (10 enums) | 1792.7 | 688 B |
