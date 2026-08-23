/**
 * @file error.hpp
 * @author llhsdmd (llhsdmd@gmail.com)
 * @brief
 * @version 0.1
 * @date 2025-06-10
 *
 * @copyright Copyright (c) 2024
 *
 */
#pragma once

#include "nekoproto/global/expected.hpp"
#include "nekoproto/global/global.hpp"
#include "nekoproto/global/log.hpp"

#include <cstdint>
#include <string>
#include <system_error>
#include <type_traits>
#include <utility>

namespace nekoproto {

namespace sa {

struct Error {
    std::error_code ec;
    std::string msg = "unknown error";

    template <typename T>
    operator expected::expected<T, Error>() const& {
        return expected::unexpected<Error>(*this);
    }

    template <typename T>
    operator expected::expected<T, Error>() && {
        return expected::unexpected<Error>(std::move(*this));
    }
};

enum class ErrorCode : std::uint8_t {
    Ok            = 0,
    Unknown       = 1,
    InvalidIndex  = 2,
    InvalidField  = 3,
    InvalidType   = 4,
    ParseError    = 5,
    InvalidLength = 6,
};

class ErrorCategory : public std::error_category {
public:
    auto name() const noexcept -> const char* override { return "serialization"; }

    auto message(int ev) const noexcept -> std::string override {
        switch (static_cast<ErrorCode>(ev)) {
        case ErrorCode::Ok:
            return "ok";
        case ErrorCode::Unknown:
            return "unknown error";
        case ErrorCode::InvalidIndex:
            return "invalid index";
        case ErrorCode::InvalidField:
            return "invalid field";
        case ErrorCode::InvalidType:
            return "invalid type";
        case ErrorCode::ParseError:
            return "parse error";
        case ErrorCode::InvalidLength:
            return "invalid length";
        default:
            return "unknown error";
        }
    }
};

inline auto makeErrorCode(ErrorCode ec) noexcept -> std::error_code {
    static ErrorCategory kCat;
    return {static_cast<int>(ec), kCat};
}

// Required by std::error_code's ADL customization protocol.
inline auto make_error_code(ErrorCode ec) noexcept -> std::error_code { // NOLINT(readability-identifier-naming)
    return makeErrorCode(ec);
}

inline auto error(std::error_code ec) -> Error { return {ec, ec.message()}; }

inline auto error(std::string msg) -> Error { return {makeErrorCode(ErrorCode::Unknown), std::move(msg)}; }

inline auto error(ErrorCode ec, std::string msg) -> Error { return {makeErrorCode(ec), std::move(msg)}; }

template <typename T>
using Result = expected::expected<T, Error>;

using Unexpected = expected::unexpected<Error>;

inline auto err(Error error) -> Unexpected { return Unexpected(std::move(error)); }

inline auto err(std::error_code ec) -> Unexpected { return err(error(ec)); }

inline auto err(std::string msg) -> Unexpected { return err(error(std::move(msg))); }

inline auto err(ErrorCode ec, std::string msg) -> Unexpected { return err(error(ec, std::move(msg))); }

template <typename T>
inline auto errorPtr(const Result<T>& result) noexcept -> const Error* {
    if (result) {
        return nullptr;
    }
    return &result.error();
}

inline auto success() -> Result<void> { return {}; }
} // namespace sa
} // namespace nekoproto

template <>
struct std::is_error_code_enum<nekoproto::sa::ErrorCode> : std::true_type {};
