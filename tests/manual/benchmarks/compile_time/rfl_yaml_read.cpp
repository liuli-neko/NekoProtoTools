#include "rfl_workload.hpp"
#include <rfl/yaml.hpp>

void compile_rfl_yaml_read(const std::string& str, rfl_compile_bench::BenchmarkStruct& value) {
    auto res = rfl::yaml::read<rfl_compile_bench::BenchmarkStruct>(str);
    if (res) {
        value = res.value();
    }
}

