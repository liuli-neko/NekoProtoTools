#include "workload.hpp"

#include "nekoproto/serialization/yaml_serializer.hpp"

#if defined(NEKO_PROTO_NO_YAML_SERIALIZER)
#error "Compile this workload with NEKO_PROTO_ENABLE_LIBFYAML or NEKO_PROTO_ENABLE_YAMLCPP"
#endif

void compile_yaml_read(const char* data, std::size_t size, neko_compile_bench::BenchmarkStruct& value) {
    nekoproto::YamlSerializer::InputSerializer input(data, size);
    (void)input(value);
}

