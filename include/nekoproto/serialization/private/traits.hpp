/**
 * @file traits.hpp
 * @author llhsdmd (llhsdmd@gmail.com)
 * @brief
 * @version 0.1
 * @date 2024-07-10
 *
 * @copyright Copyright (c) 2024
 *
 */
#pragma once
#include "nekoproto/global/global.hpp"

#include <concepts>
#include <optional>
#include <type_traits>
#include <utility>
#include <variant>

NEKO_BEGIN_NAMESPACE
namespace traits {
template <typename T, class enable = void>
struct has_monostate : std::false_type {};

template <typename... Ts>
struct has_monostate<std::variant<Ts...>, std::enable_if_t<(std::is_same_v<std::monostate, Ts> || ...)>>
    : std::true_type {};

template <typename T>
concept optional_like = requires(T t, typename T::value_type v) {
    { T() } -> std::same_as<T>;
    { T(v) } -> std::same_as<T>;
    { t.has_value() } -> std::convertible_to<bool>;
    { t.value() };
    { t = v };
    { t.reset() };
    { t.emplace() };
};

template <typename T, class enable = void>
struct optional_like_type {
    constexpr static bool value = false;
};

template <typename T>
    requires optional_like<std::remove_cvref_t<T>>
struct optional_like_type<T, void> {
    constexpr static bool value = true;
    using type = std::remove_cvref_t<T>::value_type;

    static bool has_value(const T& val) { return val.has_value(); }
    static void set_null(T& val) { val.reset(); }
    template <typename U>
    static decltype(auto) get_value(U&& self) {
        return self.value();
    }
    template <typename U>
    static void set_value(T& self, U&& val) {
        self.emplace(std::forward<U>(val));
    }
};

template <typename T>
struct optional_like_type<T, std::enable_if_t<has_monostate<std::remove_cvref_t<T>>::value>> {
    constexpr static bool value = true;
    using type = std::remove_cvref_t<T>;

    static bool has_value(const T& val) { return !std::holds_alternative<std::monostate>(val); }
    static void set_null(T& val) { val = std::monostate{}; }
    template <typename U>
    static decltype(auto) get_value(U&& self) {
        return std::forward<U>(self);
    }
    template <typename U>
    static void set_value(T& self, U&& val) {
        self = std::forward<U>(val);
    }
};

template <typename T>
using ref_type = typename std::conditional<
    std::is_array<typename std::remove_reference<T>::type>::value, typename std::remove_cv<T>::type,
    typename std::conditional<std::is_lvalue_reference<T>::value, T, typename std::decay<T>::type>::type>::type;

} // namespace traits
NEKO_END_NAMESPACE
