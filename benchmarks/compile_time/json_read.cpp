#include "workload.hpp"

#include "nekoproto/serialization/json_serializer.hpp"

#ifdef NEKO_PROTO_NO_JSON_SERIALIZER
#error "Compile this workload with NEKO_PROTO_ENABLE_RAPIDJSON or NEKO_PROTO_ENABLE_SIMDJSON"
#endif

void compile_json_read(const char* data, std::size_t size, neko_compile_bench::BenchmarkStruct& value) {
    nekoproto::JsonSerializer::InputSerializer input(data, size);
    (void)input(value);
}
