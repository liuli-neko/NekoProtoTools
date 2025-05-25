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

NEKO_BEGIN_NAMESPACE

template <size_t N>
struct string_literal { // NOLINT
    using value_type      = char;
    using reference       = value_type&;
    using const_reference = const value_type&;
    using pointer         = value_type*;
    using const_pointer   = const value_type*;
    using size_type       = size_t;

    static constexpr size_t length = (N > 0) ? (N - 1) : 0; // NOLINT

    [[nodiscard]] constexpr size_t size() const noexcept { return length; }

    constexpr string_literal() noexcept                                 = default;
    constexpr string_literal(const string_literal&) noexcept            = default;
    constexpr string_literal(string_literal&&) noexcept                 = default;
    constexpr string_literal& operator=(const string_literal&) noexcept = default;
    constexpr string_literal& operator=(string_literal&&) noexcept      = default;

    constexpr string_literal(const char (&str)[N]) noexcept {
        for (size_t i = 0; i < N; ++i) {
            value[i] = str[i];
        }
    }

    char value[N];
    constexpr const char* begin() const noexcept { return value; }
    constexpr const char* end() const noexcept { return value + length; }

    [[nodiscard]] constexpr auto operator<=>(const string_literal&) const = default;

    [[nodiscard]] constexpr std::string_view view() const noexcept { return {value, length}; }

    [[nodiscard]] constexpr operator std::string_view() const noexcept { return {value, length}; }

    constexpr reference operator[](size_type index) noexcept { return value[index]; }
    constexpr const_reference operator[](size_type index) const noexcept { return value[index]; }
};

template <size_t N>
constexpr auto string_literal_from_view(std::string_view str) {
    string_literal<N + 1> sl{};
    for (size_t i = 0; i < str.size(); ++i) {
        sl[i] = str[i];
    }
    *(sl.value + N) = '\0';
    return sl;
}

namespace detail {
template <std::array V>
struct make_static {                 // NOLINT
    static constexpr auto value = V; // NOLINT
};

template <const std::string_view&... Strs>
inline constexpr std::string_view join() {              // NOLINT
    constexpr auto joined_arr = []() {                  // NOLINT
        constexpr size_t len = (Strs.size() + ... + 0); // NOLINT
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
    auto& static_arr = make_static<joined_arr>::value; // NOLINT
    return {static_arr.data(), static_arr.size() - 1};
}
} // namespace detail

// Helper to get the value out
template <const std::string_view&... Strs>
inline constexpr auto join_v = detail::join<Strs...>(); // NOLINT

// --- ConstexprString Implementation (C++20 NTTP) ---
template <std::size_t N>
struct ConstexprString {
    std::array<char, N + 1> data{};

    // consteval 构造函数，确保编译时创建
    consteval ConstexprString(const char* str) noexcept {
        std::size_t actualLen = 0;
        // 复制直到 null 或达到 N
        while (str[actualLen] != '\0' && actualLen < N) {
            data[actualLen] = str[actualLen];
            actualLen++;
        }
        // 如果有空间，添加 null 终止符 (string_view 不需要，但 c_str 可能需要)
        if (actualLen <= N) {
            data[actualLen] = '\0';
        }
        // C++20 要求 NTTP 类型的所有基类和非静态数据成员都是 public 的
        // 并且类型是结构性相等的 (structural equality) - 默认即可
    }

    // 比较运算符对 NTTP 至关重要
    constexpr auto operator<=>(const ConstexprString&) const = default;
    constexpr bool operator==(const ConstexprString&) const  = default;

    // 访问器
    [[nodiscard]]
    constexpr std::size_t size() const noexcept {
        return N;
    }
    [[nodiscard]]
    constexpr std::string_view view() const noexcept {
        return std::string_view(data.data(), N);
    }
    [[nodiscard]]
    constexpr const char* c_str() const noexcept { // NOLINT
        return data.data();
    }
};

// CTAD 推导指引，方便从字面量创建 ConstexprString (去掉末尾 '\0')
template <std::size_t N>
ConstexprString(const char (&)[N]) -> ConstexprString<N - 1>;

NEKO_END_NAMESPACE