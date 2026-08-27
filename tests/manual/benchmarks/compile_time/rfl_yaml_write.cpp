#include "rfl_workload.hpp"
#include <rfl/yaml.hpp>

void compile_rfl_yaml_write(const rfl_compile_bench::BenchmarkStruct& value) {
    auto str = rfl::yaml::write(value);
    (void)str;
}

