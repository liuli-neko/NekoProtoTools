# JSON benchmark comparison

Median of 3 repeats, 1000 operations per case. Each row compares the same case and operation across four adapters.

The deserialize input is the same NekoProtoTools JSON fixture for every adapter. Serialize throughput uses each adapter's own output size.

## serialize

### ns/op (lower is better)

| Case | Neko / yyjson | Neko / simdjson | reflect-cpp / yyjson | RapidJSON (Handwritten) |
|---|---:|---:|---:|---:|
| small | 222.425 | 518.226 | 213.880 | 517.662 |
| medium | 661.094 | 2096.080 | 630.687 | 1111.000 |
| large | 3302.940 | 12352.500 | 3394.630 | 6231.050 |
| small_null_email | 216.585 | 471.237 | 204.601 | 574.958 |

### MB/s (higher is better)

| Case | Neko / yyjson | Neko / simdjson | reflect-cpp / yyjson | RapidJSON (Handwritten) |
|---|---:|---:|---:|---:|
| small | 750.815 | 322.253 | 780.812 | 322.604 |
| medium | 502.198 | 154.574 | 526.410 | 298.830 |
| large | 469.885 | 115.118 | 457.193 | 249.075 |
| small_null_email | 771.060 | 350.142 | 816.223 | 290.456 |

## deserialize

### ns/op (lower is better)

| Case | Neko / yyjson | Neko / simdjson | reflect-cpp / yyjson | RapidJSON (Handwritten) |
|---|---:|---:|---:|---:|
| small | 240.242 | 300.795 | 315.434 | 394.051 |
| medium | 500.263 | 614.161 | 587.132 | 739.680 |
| large | 2329.040 | 3032.070 | 2481.090 | 3104.580 |
| small_null_email | 221.358 | 299.559 | 315.051 | 394.535 |

### MB/s (higher is better)

| Case | Neko / yyjson | Neko / simdjson | reflect-cpp / yyjson | RapidJSON (Handwritten) |
|---|---:|---:|---:|---:|
| small | 695.132 | 555.195 | 529.429 | 423.803 |
| medium | 663.651 | 540.575 | 565.461 | 448.843 |
| large | 666.369 | 511.862 | 625.531 | 499.907 |
| small_null_email | 754.434 | 557.486 | 530.073 | 423.283 |

## Serialized JSON bytes

| Case | Neko / yyjson | Neko / simdjson | reflect-cpp / yyjson | RapidJSON (Handwritten) |
|---|---:|---:|---:|---:|
| small | 167 | 167 | 167 | 167 |
| medium | 332 | 324 | 332 | 332 |
| large | 1552 | 1422 | 1552 | 1552 |
| small_null_email | 167 | 165 | 167 | 167 |

[Comparison CSV](json.csv) · [Raw repeat data](raw.csv) · [Environment](environment.txt) · [Compile commands](compile-commands.txt)
