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

NEKO_END_NAMESPACE