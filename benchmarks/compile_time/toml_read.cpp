#include "workload.hpp"

#include "nekoproto/serialization/toml_serializer.hpp"

#if !defined(NEKO_PROTO_ENABLE_TOMLPLUSPLUS)
#error "Compile this workload with NEKO_PROTO_ENABLE_TOMLPLUSPLUS"
#endif

void compile_toml_read(const char* data, std::size_t size, neko_compile_bench::BenchmarkStruct& value) {
    nekoproto::TomlSerializer::InputSerializer input(data, size);
    (void)input(value);
}

