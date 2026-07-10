#pragma once

#include "nekoproto/global/global.hpp"

#include <string>
#include <string_view>
#include <system_error>
#include <utility>

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

struct ArgParserErrorDetail {
    ArgParserError error = ArgParserError::Success;
    std::string message;
};

namespace detail {
inline ArgParserErrorDetail& current_argparser_error_detail() {
    static thread_local ArgParserErrorDetail detail;
    return detail;
}

inline void clear_argparser_error_detail() { current_argparser_error_detail() = {}; }

inline std::string argparser_error_message(ArgParserError error) {
    switch (error) {
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

class ArgParserErrorCategory final : public std::error_category {
public:
    [[nodiscard]] const char* name() const noexcept override { return "nekoproto.argparser"; }
    [[nodiscard]] std::string message(int condition) const override {
        const auto error = static_cast<ArgParserError>(condition);
        return argparser_error_message(error);
    }
};

inline const std::error_category& argparser_error_category() {
    static ArgParserErrorCategory s_category;
    return s_category;
}

inline std::error_code make_argparser_error(ArgParserError error, std::string detail) {
    current_argparser_error_detail() = {.error = error, .message = std::move(detail)};
    return {static_cast<int>(error), argparser_error_category()};
}

inline std::error_code make_argparser_error(ArgParserError error, std::string_view detail) {
    return make_argparser_error(error, std::string(detail));
}

inline std::error_code make_argparser_error(ArgParserError error, const char* detail) {
    return make_argparser_error(error, std::string(detail == nullptr ? "" : detail));
}
} // namespace detail

/**
 * Returns the diagnostic produced by the most recent ArgParser operation on this thread.
 *
 * The returned value owns its message, so it remains valid after later parser calls. Use
 * std::error_code for stable programmatic handling and this function for user-facing context.
 */
[[nodiscard]] inline ArgParserErrorDetail last_error() { return detail::current_argparser_error_detail(); }

} // namespace argparser
NEKO_END_NAMESPACE

inline std::error_code make_error_code(NEKO_NAMESPACE::argparser::ArgParserError error) {
    return {static_cast<int>(error), NEKO_NAMESPACE::argparser::detail::argparser_error_category()};
}

namespace std {
template <>
struct is_error_code_enum<NEKO_NAMESPACE::argparser::ArgParserError> : true_type {};
} // namespace std
