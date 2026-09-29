#include "workload.hpp"

#include "nekoproto/serialization/xml_serializer.hpp"

#if !defined(NEKO_PROTO_ENABLE_PUGIXML)
#error "Compile this workload with NEKO_PROTO_ENABLE_PUGIXML"
#endif

void compile_xml_write(const neko_compile_bench::BenchmarkStruct& value, std::vector<char>& buffer) {
    nekoproto::XmlSerializer::OutputSerializer output(buffer);
    (void)output(value);
    (void)output.end();
}

