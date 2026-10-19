#pragma once
struct ModuleRecord {
    int id = 0;
    std::string text;
    bool operator==(const ModuleRecord&) const = default;
};
template <> struct nekoproto::Meta<ModuleRecord> {
    static constexpr auto value = nekoproto::Object("id", &ModuleRecord::id, "text", &ModuleRecord::text);
};
// The header TU returns a library type instantiated with the same user type.
auto module_header_roundtrip(const ModuleRecord&) -> nekoproto::sa::Result<ModuleRecord>;

#ifdef NEKO_TEST_PROTOCOL
#include <nekoproto/proto/macros.hpp>
struct ModuleMessage {
    int id = 0;
    NEKO_DECLARE_PROTOCOL(ModuleMessage, nekoproto::BinarySerializer)
};
template <> struct nekoproto::Meta<ModuleMessage> {
    static constexpr auto value = nekoproto::Object("id", &ModuleMessage::id);
};
#endif
