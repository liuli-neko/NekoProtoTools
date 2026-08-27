#include "workload.hpp"

#include "nekoproto/serialization/yaml_serializer.hpp"

#if defined(NEKO_PROTO_NO_YAML_SERIALIZER)
#error "Compile this workload with NEKO_PROTO_ENABLE_LIBFYAML or NEKO_PROTO_ENABLE_YAMLCPP"
#endif

void compile_yaml_write(const neko_compile_bench::BenchmarkStruct& value, std::vector<char>& buffer) {
    nekoproto::YamlSerializer::OutputSerializer output(buffer);
    (void)output(value);
    (void)output.end();
}

