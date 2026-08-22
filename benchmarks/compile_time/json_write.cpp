#include "workload.hpp"

#include "nekoproto/serialization/json_serializer.hpp"

#ifdef NEKO_PROTO_NO_JSON_SERIALIZER
#error "Compile this workload with NEKO_PROTO_ENABLE_RAPIDJSON or NEKO_PROTO_ENABLE_SIMDJSON"
#endif

void compile_json_write(const neko_compile_bench::BenchmarkStruct& value, std::vector<char>& buffer) {
    NEKO_NAMESPACE::JsonSerializer::OutputSerializer output(buffer);
    (void)output(value);
    (void)output.end();
}
