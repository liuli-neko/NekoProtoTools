#include "common/user.hpp"

#include <nekoproto/argparser/argparser.hpp>
#include <nekoproto/serialization/binary_serializer.hpp>

#include <iostream>
#include <string_view>
#include <vector>

int main(int argc, char** argv) {
    auto parsed = nekoproto::argparser::parser<User>(argc, argv);
    if (!parsed) {
        std::cerr << parsed.error().message() << '\n';
        return 1;
    }

    std::vector<char> bytes;
    nekoproto::BinarySerializer::OutputSerializer output(bytes);
    if (!output(*parsed)) return 2;
    User copy;
    nekoproto::BinarySerializer::InputSerializer input(bytes.data(), bytes.size());
    if (!input(copy)) return 3;

    nekoproto::Reflect<User>::forEach(copy, [](const auto& field, std::string_view name) {
        std::cout << name << " = " << field << '\n';
    });
    return copy.id == parsed->id && copy.name == parsed->name ? 0 : 4;
}
