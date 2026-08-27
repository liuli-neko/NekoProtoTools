#include "rfl_workload.hpp"
#include <rfl/json.hpp>

void compile_rfl_json_write(const rfl_compile_bench::BenchmarkStruct& value) {
    auto str = rfl::json::write(value);
    (void)str;
}

