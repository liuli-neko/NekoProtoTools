#include "rfl_workload.hpp"
#include <rfl/json.hpp>

void compile_rfl_json_read(const std::string& str, rfl_compile_bench::BenchmarkStruct& value) {
    auto res = rfl::json::read<rfl_compile_bench::BenchmarkStruct>(str);
    if (res) {
        value = res.value();
    }
}

