#include <nekoproto/serialization.hpp>
#ifdef NEKO_TEST_PROTOCOL
#include <nekoproto/proto/proto_base.hpp>
#endif
#include "component_shared.hpp"

auto module_header_roundtrip(const ModuleRecord& source) -> nekoproto::sa::Result<ModuleRecord> {
    std::vector<char> data;
    nekoproto::BinarySerializer::OutputSerializer output(data);
    if (!output(source) || !output.end()) return nekoproto::sa::err("encode");
    nekoproto::BinarySerializer::InputSerializer input(data.data(), data.size());
    ModuleRecord result;
    if (!input(result)) return nekoproto::sa::err("decode");
    return result;
}
