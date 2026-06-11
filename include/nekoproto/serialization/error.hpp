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

#include <system_error>

NEKO_BEGIN_NAMESPACE

namespace sa {
struct Error {
    std::error_code ec;
    std::string msg = "unknown error";
};

enum class ErrorCode : std::uint8_t {
    Ok           = 0,
    Unknown      = 1,
    InvalidIndex = 2,
    InvalidField = 3,
    InvalidType  = 4,
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
        default:
            return "unknown error";
        }
    }
};

template <typename T>
class Result {
public:
    Result() = default;
    Result(T&& value) : mData(std::move(value)) {}
    Result(const T& value) : mData(value) {}
    Result(const Error& error) : mData(error) {}
    Result(Error&& error) : mData(std::move(error)) {}
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

    auto operator*() const -> T {
        if (auto* ptr = std::get_if<T>(&mData)) {
            return *ptr;
        }
        throw std::runtime_error("invalid result");
    }

    auto operator->() const -> T* {
        if (auto* ptr = std::get_if<T>(&mData)) {
            return ptr;
        }
        throw std::runtime_error("invalid result");
    }

    operator bool() const { return std::holds_alternative<T>(mData); }

    auto hasError() const -> bool { return std::holds_alternative<Error>(mData); }

    auto error() const -> Error {
        if (auto* ptr = std::get_if<Error>(&mData)) {
            return *ptr;
        }
        throw std::runtime_error("invalid result");
    }

    auto hasValue() const -> bool { return std::holds_alternative<T>(mData); }

    auto value() const -> T {
        if (auto* ptr = std::get_if<T>(&mData)) {
            return *ptr;
        }
        throw std::runtime_error("invalid result");
    }

private:
    std::variant<T, Error> mData;
};

auto make_error_code(ErrorCode ec) noexcept -> std::error_code {
    static ErrorCategory kCat;
    return {static_cast<int>(ec), kCat};
}

inline auto error(std::error_code ec) -> Error { return {ec, ec.message()}; }

inline auto error(std::string msg) -> Error { return {make_error_code(ErrorCode::Unknown), std::move(msg)}; }

inline auto error(ErrorCode ec, std::string msg) -> Error { return {make_error_code(ec), std::move(msg)}; }
} // namespace sa
NEKO_END_NAMESPACE

template <>
struct std::is_error_code_enum<NEKO_NAMESPACE::sa::ErrorCode> : std::true_type {};
