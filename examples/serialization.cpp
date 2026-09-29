#include "common/user.hpp"

#include <nekoproto/serialization/binary_serializer.hpp>
#if defined(NEKO_PROTO_ENABLE_RAPIDJSON)
#include <nekoproto/serialization/json_serializer.hpp>
#endif

#include <iostream>
#include <vector>

int main() {
    const User source{7, "Neko"};
    std::vector<char> binary;
    nekoproto::BinarySerializer::OutputSerializer output(binary);
    if (!output(source)) return 1;

    User decoded;
    nekoproto::BinarySerializer::InputSerializer input(binary.data(), binary.size());
    if (!input(decoded) || decoded.id != source.id || decoded.name != source.name) return 2;
    std::cout << "Binary round trip: " << decoded.id << ' ' << decoded.name << '\n';

#if defined(NEKO_PROTO_ENABLE_RAPIDJSON)
    std::vector<char> json;
    nekoproto::JsonSerializer::OutputSerializer json_output(json);
    if (!json_output(source)) return 3;
    json_output.end();
    User from_json;
    nekoproto::JsonSerializer::InputSerializer json_input(json.data(), json.size());
    if (!json_input(from_json) || from_json.id != source.id || from_json.name != source.name) return 4;
    std::cout << "JSON: " << std::string(json.begin(), json.end()) << '\n';
#endif
}
