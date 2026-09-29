#include "rfl_workload.hpp"
#include <rfl/xml.hpp>

void compile_rfl_xml_read(const std::string& str, rfl_compile_bench::BenchmarkStruct& value) {
    auto res = rfl::xml::read<rfl_compile_bench::BenchmarkStruct>(str);
    if (res) {
        value = res.value();
    }
}

