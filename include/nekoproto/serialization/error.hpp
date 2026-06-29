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

#include "nekoproto/global/global.hpp"
#include "nekoproto/global/log.hpp"

#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <system_error>
#include <type_traits>
#include <utility>
#include <variant>
#if defined(__has_include)
    #if __has_include(<expected>)
        #include <expected>
    #endif
#endif

NEKO_BEGIN_NAMESPACE

namespace sa {

#if defined(__cpp_lib_expected) && __cpp_lib_expected >= 202202L
#define NEKO_SA_USE_STD_EXPECTED 1
#else
#define NEKO_SA_USE_STD_EXPECTED 0
#endif

struct Error {
    std::error_code ec;
    std::string msg = "unknown error";

#if NEKO_SA_USE_STD_EXPECTED
    template <typename T>
    operator std::expected<T, Error>() const& {
        return std::unexpected<Error>(*this);
    }

    template <typename T>
    operator std::expected<T, Error>() && {
        return std::unexpected<Error>(std::move(*this));
    }
#endif
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
    const char* name() const noexcept override { return "serialization"; }

    std::string message(int ev) const noexcept override {
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

inline auto make_error_code(ErrorCode ec) noexcept -> std::error_code {
    static ErrorCategory kCat;
    return {static_cast<int>(ec), kCat};
}

inline auto error(std::error_code ec) -> Error { return {ec, ec.message()}; }

inline auto error(std::string msg) -> Error { return {make_error_code(ErrorCode::Unknown), std::move(msg)}; }

inline auto error(ErrorCode ec, std::string msg) -> Error { return {make_error_code(ec), std::move(msg)}; }

#if NEKO_SA_USE_STD_EXPECTED

template <typename T>
using Result = std::expected<T, Error>;

using Unexpected = std::unexpected<Error>;

#else

class Unexpected {
public:
    explicit Unexpected(Error error) : mError(std::move(error)) {}

    auto error() const& noexcept -> const Error& { return mError; }
    auto error() && noexcept -> Error&& { return std::move(mError); }

private:
    Error mError;
};

template <typename T>
class Result {
public:
    Result() = default;
    Result(T&& value) : mData(std::move(value)) {}
    Result(const T& value) : mData(value) {}
    Result(const Error& error) : mData(error) {}
    Result(Error&& error) : mData(std::move(error)) {}
    Result(const Unexpected& unexpected) : mData(unexpected.error()) {}
    Result(Unexpected&& unexpected) : mData(std::move(unexpected).error()) {}
    Result(const Result&)            = default;
    Result(Result&&)                 = default;
    Result& operator=(const Result&) = default;
    Result& operator=(Result&&)      = default;

    auto operator=(T&& value) -> Result& {
        mData = std::move(value);
        return *this;
    }

    auto operator=(const Error& error) -> Result& {
        mData = error;
        return *this;
    }

    auto operator=(Error&& error) -> Result& {
        mData = std::move(error);
        return *this;
    }

    auto operator=(const Unexpected& unexpected) -> Result& {
        mData = unexpected.error();
        return *this;
    }

    auto operator=(Unexpected&& unexpected) -> Result& {
        mData = std::move(unexpected).error();
        return *this;
    }

    auto operator*() & -> T& {
        if (auto* ptr = std::get_if<T>(&mData)) {
            return *ptr;
        }
        throw std::runtime_error("bad access to Result without a value");
    }

    auto operator*() const& -> const T& {
        if (auto* ptr = std::get_if<T>(&mData)) {
            return *ptr;
        }
        throw std::runtime_error("bad access to Result without a value");
    }

    auto operator*() && -> T&& {
        if (auto* ptr = std::get_if<T>(&mData)) {
            return std::move(*ptr);
        }
        throw std::runtime_error("bad access to Result without a value");
    }

    auto operator->() -> T* {
        if (auto* ptr = std::get_if<T>(&mData)) {
            return ptr;
        }
        throw std::runtime_error("bad access to Result without a value");
    }

    auto operator->() const -> const T* {
        if (auto* ptr = std::get_if<T>(&mData)) {
            return ptr;
        }
        throw std::runtime_error("bad access to Result without a value");
    }

    operator bool() const { return std::holds_alternative<T>(mData); }

    auto hasError() const -> bool { return std::holds_alternative<Error>(mData); }
    auto has_value() const -> bool { return std::holds_alternative<T>(mData); }

    auto error() & -> Error& {
        if (auto* ptr = std::get_if<Error>(&mData)) {
            return *ptr;
        }
        throw std::runtime_error("Result does not contain an error");
    }

    auto error() const& -> const Error& {
        if (auto* ptr = std::get_if<Error>(&mData)) {
            return *ptr;
        }
        throw std::runtime_error("Result does not contain an error");
    }

    auto error() && -> Error&& {
        if (auto* ptr = std::get_if<Error>(&mData)) {
            return std::move(*ptr);
        }
        throw std::runtime_error("Result does not contain an error");
    }

    auto hasValue() const -> bool { return std::holds_alternative<T>(mData); }

    auto errorPtr() const noexcept -> const Error* { return std::get_if<Error>(&mData); }

    auto value() & -> T& {
        if (auto* ptr = std::get_if<T>(&mData)) {
            return *ptr;
        }
        throw std::runtime_error("bad access to Result without a value");
    }

    auto value() const& -> const T& {
        if (auto* ptr = std::get_if<T>(&mData)) {
            return *ptr;
        }
        throw std::runtime_error("bad access to Result without a value");
    }

    auto value() && -> T&& {
        if (auto* ptr = std::get_if<T>(&mData)) {
            return std::move(*ptr);
        }
        throw std::runtime_error("bad access to Result without a value");
    }

private:
    std::variant<T, Error> mData;
};

template <>
class Result<void> {
public:
    Result() = default;
    Result(const Error& error) : mError(error) {}
    Result(Error&& error) : mError(std::move(error)) {}
    Result(const Unexpected& unexpected) : mError(unexpected.error()) {}
    Result(Unexpected&& unexpected) : mError(std::move(unexpected).error()) {}

    auto operator=(const Error& error) -> Result& {
        mError = error;
        return *this;
    }

    auto operator=(Error&& error) -> Result& {
        mError = std::move(error);
        return *this;
    }

    auto operator=(const Unexpected& unexpected) -> Result& {
        mError = unexpected.error();
        return *this;
    }

    auto operator=(Unexpected&& unexpected) -> Result& {
        mError = std::move(unexpected).error();
        return *this;
    }

    operator bool() const noexcept { return !mError.has_value(); }

    auto hasError() const noexcept -> bool { return mError.has_value(); }
    auto has_value() const noexcept -> bool { return !mError.has_value(); }
    auto hasValue() const noexcept -> bool { return !mError.has_value(); }

    auto value() const -> void {
        if (mError) {
            throw std::runtime_error("bad access to Result without a value");
        }
    }

    auto error() & -> Error& {
        if (mError) {
            return *mError;
        }
        throw std::runtime_error("Result does not contain an error");
    }

    auto error() const& -> const Error& {
        if (mError) {
            return *mError;
        }
        throw std::runtime_error("Result does not contain an error");
    }

    auto error() && -> Error&& {
        if (mError) {
            return std::move(*mError);
        }
        throw std::runtime_error("Result does not contain an error");
    }

    auto errorPtr() const noexcept -> const Error* {
        if (mError) {
            return &*mError;
        }
        return nullptr;
    }

private:
    std::optional<Error> mError;
};

#endif

inline auto Err(Error error) -> Unexpected { return Unexpected(std::move(error)); }

inline auto Err(std::error_code ec) -> Unexpected { return Err(error(ec)); }

inline auto Err(std::string msg) -> Unexpected { return Err(error(std::move(msg))); }

inline auto Err(ErrorCode ec, std::string msg) -> Unexpected { return Err(error(ec, std::move(msg))); }

template <typename T>
inline auto error_ptr(const Result<T>& result) noexcept -> const Error* {
    if (result) {
        return nullptr;
    }
    return &result.error();
}

inline auto success() -> Result<void> { return {}; }
} // namespace sa
NEKO_END_NAMESPACE

template <>
struct std::is_error_code_enum<NEKO_NAMESPACE::sa::ErrorCode> : std::true_type {};
