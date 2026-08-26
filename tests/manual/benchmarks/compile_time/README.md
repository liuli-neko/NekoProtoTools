# Reflection compile-time benchmark

This suite isolates reflection include cost, `Reflect<T>::forEach`, binary and JSON read/write,
schema generation, automatic enum scanning, field-count scaling (4/16/32/64), and three tag
densities. The reflected workload mixes arithmetic values, strings, optionals, vectors, enums,
and nested reflected objects.

Run from the repository root with Clang:

```powershell
.\benchmarks\compile_time\run.ps1 -Label local -Repeats 3 -Trace
.\benchmarks\compile_time\analyze_trace.ps1 -TraceDirectory .\build\compile_time\local
```

The runner uses the MSYS2 clang64 compiler by default and discovers a RapidJSON installation
from xmake's local package cache. Override `-Compiler` or `-RapidJsonInclude` when necessary.
Each label gets a separate directory containing object files, Clang time-trace JSON, wall-clock
CSV data, and optional trace summaries. `-ProjectRoot` and `-OutputRoot` allow the same workload
to be run against an archived baseline include tree while keeping output elsewhere.
