#pragma once

#include "nekoproto/global/global.hpp"

#include <system_error>

NEKO_BEGIN_NAMESPACE
namespace argparser {

enum class ArgParserError {
    Success = 0,
    UnknownOption,
    MissingValue,
    InvalidValue,
    MissingRequired,
    UnexpectedPositional,
    InvalidDefinition,
    HelpRequested,
    UnknownCommand,
    VersionRequested,
};

namespace detail {
class ArgParserErrorCategory final : public std::error_category {
public:
    [[nodiscard]] const char* name() const noexcept override { return "nekoproto.argparser"; }
    [[nodiscard]] std::string message(int condition) const override {
        switch (static_cast<ArgParserError>(condition)) {
        case ArgParserError::Success:
            return "success";
        case ArgParserError::UnknownOption:
            return "unknown option";
        case ArgParserError::MissingValue:
            return "missing option value";
        case ArgParserError::InvalidValue:
            return "invalid option value";
        case ArgParserError::MissingRequired:
            return "missing required option";
        case ArgParserError::UnexpectedPositional:
            return "unexpected positional argument";
        case ArgParserError::InvalidDefinition:
            return "invalid argument definition";
        case ArgParserError::HelpRequested:
            return "help requested";
        case ArgParserError::UnknownCommand:
            return "unknown command";
        case ArgParserError::VersionRequested:
            return "version requested";
        }
        return "unknown argparser error";
    }
};

inline const std::error_category& argparser_error_category() {
    static ArgParserErrorCategory s_category;
    return s_category;
}
} // namespace detail
} // namespace argparser
NEKO_END_NAMESPACE

inline std::error_code make_error_code(NEKO_NAMESPACE::argparser::ArgParserError error) {
    return {static_cast<int>(error), NEKO_NAMESPACE::argparser::detail::argparser_error_category()};
}

namespace std {
template <>
struct is_error_code_enum<NEKO_NAMESPACE::argparser::ArgParserError> : true_type {};
} // namespace std