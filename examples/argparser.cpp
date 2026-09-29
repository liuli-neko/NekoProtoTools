#include "common/user.hpp"

#include <nekoproto/argparser/argparser.hpp>

#include <iostream>

int main(int argc, char** argv) {
    auto parsed = nekoproto::argparser::parser<User>(argc, argv);
    if (!parsed) {
        if (parsed.error() == nekoproto::argparser::makeErrorCode(nekoproto::argparser::ArgParserError::HelpRequested)) {
            std::cout << nekoproto::argparser::formatHelp<User>() << '\n';
            return 0;
        }
        std::cerr << parsed.error().message() << '\n';
        return 1;
    }
    std::cout << parsed->id << ' ' << parsed->name << '\n';
}
