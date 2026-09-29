#include "workload.hpp"

#include "nekoproto/serialization/toml_serializer.hpp"

#if !defined(NEKO_PROTO_ENABLE_TOMLPLUSPLUS)
#error "Compile this workload with NEKO_PROTO_ENABLE_TOMLPLUSPLUS"
#endif

void compile_toml_write(const neko_compile_bench::BenchmarkStruct& value, std::vector<char>& buffer) {
    nekoproto::TomlSerializer::OutputSerializer output(buffer);
    (void)output(value);
    (void)output.end();
}

