#include "workload.hpp"

#include "nekoproto/serialization/parsing/parsers.hpp"

NEKO_NAMESPACE::parsing::schema::Type compile_schema() {
    return NEKO_NAMESPACE::parser_schema<neko_compile_bench::BenchmarkStruct>();
}
