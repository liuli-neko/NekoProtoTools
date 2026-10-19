#include <nekoproto/serialization/json_serializer.hpp>
#include <nekoproto/serialization/binary_serializer.hpp>
#include <nekoproto/serialization/yaml_serializer.hpp>
#include <nekoproto/serialization/toml_serializer.hpp>
#include <nekoproto/serialization/xml_serializer.hpp>
#include <nekoproto/argparser/argparser.hpp>
#include <nekoproto/proto/private/reflection_serializer.hpp>

using namespace nekoproto;
struct Record {
    [[=rename_tag<"wire_id">]] int id = 0;
    std::string label;
    bool operator==(const Record&) const = default;
};
struct LegacyRecord {
    int id = 0;
    std::string label;
    NEKO_SERIALIZER((makeTags<rename_tag<"wire_id">>(id)), label)
};
struct Options {
    [[=argparser::arg_name<"count", 'c'>]] int value = 0;
};

template <class S>
bool roundTrip() {
    Record source{7, "native"};
    LegacyRecord legacy{7, "native"};
    std::vector<char> data, expected;
    typename S::OutputSerializer output(data), legacy_output(expected);
    if (!output(source) || !output.end() || !legacy_output(legacy) || !legacy_output.end()) return false;
    if (data != expected) return false;
    typename S::InputSerializer input(data.data(), data.size());
    Record decoded;
    return input(decoded) && decoded == source;
}

int main() {
    if (!roundTrip<JsonSerializer>()) return 1;
    if (!roundTrip<BinarySerializer>()) return 2;
    if (!roundTrip<YamlSerializer>()) return 3;
    if (!roundTrip<TomlSerializer>()) return 4;
    Record source{7, "native"}, decoded;
    std::vector<char> xml;
    PugiXmlOutputSerializer output(xml, "record");
    if (!output(source) || !output.end()) return 5;
    PugiXmlInputSerializer input(xml.data(), xml.size());
    if (!input(decoded) || decoded != source) return 6;
    const char* argv[] = {"native", "--count", "9"};
    auto options = argparser::parser<Options>(3, argv);
    if (!options || options->value != 9) return 7;
    auto reflected = ReflectionSerializer::reflection(source);
    if (reflected.getObject()->getField<int>("id", 0) != 7) return 8;
}
