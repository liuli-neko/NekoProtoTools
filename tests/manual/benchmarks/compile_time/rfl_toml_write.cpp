#include "rfl_workload.hpp"
#include <rfl/toml.hpp>

void compile_rfl_toml_write(const rfl_compile_bench::BenchmarkStruct& value) {
    auto str = rfl::toml::write(value);
    (void)str;
}

