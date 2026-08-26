#include "workload.hpp"

#include "nekoproto/serialization/parsing/parsers.hpp"

nekoproto::parsing::schema::Type compile_schema() {
    return nekoproto::parserSchema<neko_compile_bench::BenchmarkStruct>();
}
