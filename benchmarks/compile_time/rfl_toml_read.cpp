#include "rfl_workload.hpp"
#include <rfl/toml.hpp>

void compile_rfl_toml_read(const std::string& str, rfl_compile_bench::BenchmarkStruct& value) {
    auto res = rfl::toml::read<rfl_compile_bench::BenchmarkStruct>(str);
    if (res) {
        value = res.value();
    }
}

