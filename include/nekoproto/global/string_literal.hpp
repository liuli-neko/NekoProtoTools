/**
 * @file string_literal.hpp
 * @author llhsdmd (llhsdmd@gmail.com)
 * @brief
 * @version 0.1
 * @date 2025-04-28
 *
 * @copyright Copyright (c) 2025
 *
 */
#pragma once

#include "global.hpp"

#include <array>

namespace nekoproto {

template <size_t N>
struct StringLiteral {
    using value_type      = char;
    using reference       = value_type&;
    using const_reference = const value_type&;
    using pointer         = value_type*;
    using const_pointer   = const value_type*;
    using size_type       = size_t;

    static constexpr size_t length = (N > 0) ? (N - 1) : 0;

    [[nodiscard]] constexpr auto size() const noexcept -> size_t { return length; }

    constexpr StringLiteral() noexcept                                        = default;
    constexpr StringLiteral(const StringLiteral&) noexcept                    = default;
    constexpr StringLiteral(StringLiteral&&) noexcept                         = default;
    constexpr auto operator=(const StringLiteral&) noexcept -> StringLiteral& = default;
    constexpr auto operator=(StringLiteral&&) noexcept -> StringLiteral&      = default;

    constexpr StringLiteral(const char (&str)[N]) noexcept {
        for (size_t i = 0; i < N; ++i) {
            value[i] = str[i];
        }
    }

    char value[N];
    constexpr auto begin() const noexcept -> const char* { return value; }
    constexpr auto end() const noexcept -> const char* { return value + length; }

    [[nodiscard]] constexpr auto operator<=>(const StringLiteral&) const = default;

    [[nodiscard]] constexpr auto view() const noexcept -> std::string_view { return {value, length}; }

    [[nodiscard]] constexpr operator std::string_view() const noexcept { return {value, length}; }

    constexpr auto operator[](size_type index) noexcept -> reference { return value[index]; }
    constexpr auto operator[](size_type index) const noexcept -> const_reference { return value[index]; }
};

template <size_t N>
constexpr auto stringLiteralFromView(std::string_view str) {
    StringLiteral<N + 1> sl{};
    for (size_t i = 0; i < str.size(); ++i) {
        sl[i] = str[i];
    }
    *(sl.value + N) = '\0';
    return sl;
}

namespace detail {
template <std::array V>
struct MakeStatic {
    static constexpr auto value = V;
};

template <const std::string_view&... Strs>
inline constexpr auto join() -> std::string_view {
    constexpr auto joined_arr = []() {
        constexpr size_t len = (Strs.size() + ... + 0);
        std::array<char, len + 1> arr;
        auto append = [idx = 0, &arr](const auto& src) mutable {
            for (auto ch : src) {
                arr[idx++] = ch;
            }
        };
        (append(Strs), ...);
        arr[len] = '\0';
        return arr;
    }();
    auto& static_arr = MakeStatic<joined_arr>::value;
    return {static_arr.data(), static_arr.size() - 1};
}
} // namespace detail

// Helper to get the value out
template <const std::string_view&... Strs>
inline constexpr auto join_v = detail::join<Strs...>();

// --- ConstexprString Implementation (C++20 NTTP) ---
template <std::size_t N>
struct ConstexprString {
    std::array<char, N + 1> data{};

    // consteval 构造函数，确保编译时创建
    consteval ConstexprString(const char* str) noexcept {
        std::size_t actual_len = 0;
        // 复制直到 null 或达到 N
        while (str[actual_len] != '\0' && actual_len < N) {
            data[actual_len] = str[actual_len];
            actual_len++;
        }
        // 如果有空间，添加 null 终止符 (string_view 不需要，但 c_str 可能需要)
        if (actual_len <= N) {
            data[actual_len] = '\0';
        }
        // C++20 要求 NTTP 类型的所有基类和非静态数据成员都是 public 的
        // 并且类型是结构性相等的 (structural equality) - 默认即可
    }

    // 比较运算符对 NTTP 至关重要
    constexpr auto operator<=>(const ConstexprString&) const        = default;
    constexpr auto operator==(const ConstexprString&) const -> bool = default;

    // 访问器
    [[nodiscard]]
    constexpr auto size() const noexcept -> std::size_t {
        return N;
    }
    [[nodiscard]]
    constexpr auto view() const noexcept -> std::string_view {
        return std::string_view(data.data(), N);
    }
    [[nodiscard]]
    constexpr auto c_str() const noexcept -> const char* {
        return data.data();
    }
};

// CTAD 推导指引，方便从字面量创建 ConstexprString (去掉末尾 '\0')
template <std::size_t N>
ConstexprString(const char (&)[N]) -> ConstexprString<N - 1>;

template <ConstexprString Str>
[[nodiscard]]
consteval auto operator""_cs() noexcept {
    return Str;
}

template <typename T, class enable = void>
struct IsConstexprString : std::false_type {};

template <std::size_t N>
struct IsConstexprString<ConstexprString<N>, void> : std::true_type {};

} // namespace nekoproto
