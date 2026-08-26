#include "workload.hpp"

using neko_compile_bench::BenchmarkStruct;

void compile_foreach(BenchmarkStruct& value, const BenchmarkStruct& constValue) {
    nekoproto::Reflect<BenchmarkStruct>::forEach(
        value, []<typename Field, typename Tags>(Field&, std::string_view, const Tags&) {});
    nekoproto::Reflect<BenchmarkStruct>::forEach(
        constValue, []<typename Field, typename Tags>(const Field&, std::string_view, const Tags&) {});
}
