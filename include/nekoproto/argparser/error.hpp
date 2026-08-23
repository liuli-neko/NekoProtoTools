#pragma once

#include "nekoproto/global/global.hpp"

#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace nekoproto {
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
inline auto currentArgparserErrorDetail() -> ArgParserErrorDetail& {
    static thread_local ArgParserErrorDetail detail;
    return detail;
}

inline void clearArgparserErrorDetail() { currentArgparserErrorDetail() = {}; }

inline auto argparserErrorMessage(ArgParserError error) -> std::string {
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
    [[nodiscard]] auto name() const noexcept -> const char* override { return "nekoproto.argparser"; }
    [[nodiscard]] auto message(int condition) const -> std::string override {
        const auto error = static_cast<ArgParserError>(condition);
        return argparserErrorMessage(error);
    }
};

inline auto argparserErrorCategory() -> const std::error_category& {
    static ArgParserErrorCategory s_category;
    return s_category;
}

inline auto makeArgparserError(ArgParserError error, std::string detail) -> std::error_code {
    currentArgparserErrorDetail() = {.error = error, .message = std::move(detail)};
    return {static_cast<int>(error), argparserErrorCategory()};
}

inline auto makeArgparserError(ArgParserError error, std::string_view detail) -> std::error_code {
    return makeArgparserError(error, std::string(detail));
}

inline auto makeArgparserError(ArgParserError error, const char* detail) -> std::error_code {
    return makeArgparserError(error, std::string(detail == nullptr ? "" : detail));
}
} // namespace detail

/**
 * Returns the diagnostic produced by the most recent ArgParser operation on this thread.
 *
 * The returned value owns its message, so it remains valid after later parser calls. Use
 * std::error_code for stable programmatic handling and this function for user-facing context.
 */
[[nodiscard]] inline auto lastError() -> ArgParserErrorDetail { return detail::currentArgparserErrorDetail(); }

inline auto makeErrorCode(ArgParserError error) -> std::error_code {
    return {static_cast<int>(error), detail::argparserErrorCategory()};
}

// Required by std::error_code's ADL customization protocol.
inline auto make_error_code(ArgParserError error) -> std::error_code { // NOLINT(readability-identifier-naming)
    return makeErrorCode(error);
}

} // namespace argparser
} // namespace nekoproto

namespace std {
template <>
struct is_error_code_enum<nekoproto::argparser::ArgParserError> : true_type {};
} // namespace std
