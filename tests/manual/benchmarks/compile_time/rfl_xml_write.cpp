#include "rfl_workload.hpp"
#include <rfl/xml.hpp>

void compile_rfl_xml_write(const rfl_compile_bench::BenchmarkStruct& value) {
    auto str = rfl::xml::write(value);
    (void)str;
}

