# JSON benchmark comparison

Median of 3 repeats, 1000 operations per case. Each row compares the same case and operation across 5 adapters.

The deserialize input is the same NekoProtoTools JSON fixture for every adapter. Serialize throughput uses each adapter's own output size.

## serialize

### ns/op (lower is better)

| Case | Neko / yyjson | Neko / simdjson | reflect-cpp / yyjson | RapidJSON (Handwritten) | Glaze |
|---|---:|---:|---:|---:|---:|
| small | 219.730 | 497.567 | 211.017 | 543.228 | 90.115 |
| medium | 637.274 | 1966.930 | 640.780 | 1144.030 | 511.269 |
| large | 3325.250 | 12484.200 | 3392.770 | 5854.090 | 3508.490 |
| small_null_email | 208.988 | 487.469 | 206.085 | 551.310 | 79.664 |

### MB/s (higher is better)

| Case | Neko / yyjson | Neko / simdjson | reflect-cpp / yyjson | RapidJSON (Handwritten) | Glaze |
|---|---:|---:|---:|---:|---:|
| small | 760.024 | 335.633 | 791.405 | 307.422 | 1853.190 |
| medium | 520.969 | 164.723 | 518.119 | 290.203 | 633.717 |
| large | 466.731 | 113.904 | 457.444 | 265.114 | 405.303 |
| small_null_email | 799.089 | 338.483 | 810.345 | 302.915 | 2071.200 |

## deserialize

### ns/op (lower is better)

| Case | Neko / yyjson | Neko / simdjson | reflect-cpp / yyjson | RapidJSON (Handwritten) | Glaze |
|---|---:|---:|---:|---:|---:|
| small | 228.286 | 300.232 | 317.252 | 375.500 | 150.093 |
| medium | 483.230 | 597.274 | 561.272 | 769.734 | 435.147 |
| large | 2411.650 | 2947.930 | 2447.090 | 3138.910 | 2397.170 |
| small_null_email | 228.330 | 296.979 | 322.257 | 413.454 | 160.062 |

### MB/s (higher is better)

| Case | Neko / yyjson | Neko / simdjson | reflect-cpp / yyjson | RapidJSON (Handwritten) | Glaze |
|---|---:|---:|---:|---:|---:|
| small | 731.539 | 556.237 | 526.395 | 444.740 | 1112.640 |
| medium | 687.043 | 555.859 | 591.514 | 431.318 | 762.961 |
| large | 643.543 | 526.471 | 634.223 | 494.439 | 647.431 |
| small_null_email | 731.398 | 562.329 | 518.220 | 403.914 | 1043.350 |

## Serialized JSON bytes

| Case | Neko / yyjson | Neko / simdjson | reflect-cpp / yyjson | RapidJSON (Handwritten) | Glaze |
|---|---:|---:|---:|---:|---:|
| small | 167 | 167 | 167 | 167 | 167 |
| medium | 332 | 324 | 332 | 332 | 324 |
| large | 1552 | 1422 | 1552 | 1552 | 1422 |
| small_null_email | 167 | 165 | 167 | 167 | 165 |

[Comparison CSV](json.csv) · [Raw repeat data](raw.csv) · [Environment](environment.txt) · [Compile commands](compile-commands.txt)
