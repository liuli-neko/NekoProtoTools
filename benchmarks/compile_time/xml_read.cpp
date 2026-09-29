#include "workload.hpp"

#include "nekoproto/serialization/xml_serializer.hpp"

#if !defined(NEKO_PROTO_ENABLE_PUGIXML)
#error "Compile this workload with NEKO_PROTO_ENABLE_PUGIXML"
#endif

void compile_xml_read(const char* data, std::size_t size, neko_compile_bench::BenchmarkStruct& value) {
    nekoproto::XmlSerializer::InputSerializer input(data, size);
    (void)input(value);
}

