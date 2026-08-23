/**
 * @file traits.hpp
 * @author llhsdmd (llhsdmd@gmail.com)
 * @brief Common type traits shared by NekoProto modules.
 * @version 0.1
 * @date 2026-07-09
 *
 * @copyright Copyright (c) 2026
 *
 */
#pragma once
#include "nekoproto/global/global.hpp"

#include <array>
#include <concepts>
#include <cstddef>
#include <deque>
#include <list>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

namespace nekoproto {
namespace traits {
template <typename T, class enable = void>
struct HasMonostate : std::false_type {};

template <typename... Ts>
struct HasMonostate<std::variant<Ts...>, std::enable_if_t<(std::is_same_v<std::monostate, Ts> || ...)>>
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
struct OptionalLikeType {
    constexpr static bool value = false;
};

template <typename T>
    requires optional_like<std::remove_cvref_t<T>>
struct OptionalLikeType<T, void> {
    constexpr static bool value = true;
    using type = std::remove_cvref_t<T>::value_type;

    static auto hasValue(const T& val) -> bool { return val.has_value(); }
    static void setNull(T& val) { val.reset(); }
    template <typename U>
    static auto getValue(U&& self) -> decltype(auto) {
        return self.value();
    }
    template <typename U>
    static void setValue(T& self, U&& val) {
        self.emplace(std::forward<U>(val));
    }
};

template <typename T>
struct OptionalLikeType<T, std::enable_if_t<HasMonostate<std::remove_cvref_t<T>>::value>> {
    constexpr static bool value = true;
    using type = std::remove_cvref_t<T>;

    static auto hasValue(const T& val) -> bool { return !std::holds_alternative<std::monostate>(val); }
    static void setNull(T& val) { val = std::monostate{}; }
    template <typename U>
    static auto getValue(U&& self) -> decltype(auto) {
        return std::forward<U>(self);
    }
    template <typename U>
    static void setValue(T& self, U&& val) {
        self = std::forward<U>(val);
    }
};

template <typename T>
using ref_type = typename std::conditional<
    std::is_array<typename std::remove_reference<T>::type>::value, typename std::remove_cv<T>::type,
    typename std::conditional<std::is_lvalue_reference<T>::value, T, typename std::decay<T>::type>::type>::type;

template <typename T>
inline constexpr bool is_string_like_v = // NOLINT
    std::is_same_v<std::remove_cvref_t<T>, std::string> || std::is_same_v<std::remove_cvref_t<T>, std::string_view> ||
    std::is_convertible_v<std::remove_cvref_t<T>, std::string_view>;

template <typename T, bool OptionalLike = OptionalLikeType<std::remove_cvref_t<T>>::value>
struct UnwrappedOptionalLikeType {
    using type = std::remove_cvref_t<T>;
};

template <typename T>
struct UnwrappedOptionalLikeType<T, true> {
    using type = typename OptionalLikeType<std::remove_cvref_t<T>>::type;
};

template <typename T>
using unwrapped_optional_like_type_t = std::remove_cvref_t<typename UnwrappedOptionalLikeType<T>::type>;

template <typename T>
struct IsKnownCollection : std::false_type {};

template <typename T, typename Alloc>
struct IsKnownCollection<std::vector<T, Alloc>> : std::true_type {};

template <typename T, typename Alloc>
struct IsKnownCollection<std::deque<T, Alloc>> : std::true_type {};

template <typename T, typename Alloc>
struct IsKnownCollection<std::list<T, Alloc>> : std::true_type {};

template <typename T, typename Compare, typename Alloc>
struct IsKnownCollection<std::set<T, Compare, Alloc>> : std::true_type {};

template <typename T, typename Compare, typename Alloc>
struct IsKnownCollection<std::multiset<T, Compare, Alloc>> : std::true_type {};

template <typename T, typename Hash, typename Eq, typename Alloc>
struct IsKnownCollection<std::unordered_set<T, Hash, Eq, Alloc>> : std::true_type {};

template <typename T, typename Hash, typename Eq, typename Alloc>
struct IsKnownCollection<std::unordered_multiset<T, Hash, Eq, Alloc>> : std::true_type {};

template <typename T, std::size_t N>
struct IsKnownCollection<std::array<T, N>> : std::true_type {};

template <typename K, typename V, typename Compare, typename Alloc>
struct IsKnownCollection<std::map<K, V, Compare, Alloc>> : std::true_type {};

template <typename K, typename V, typename Compare, typename Alloc>
struct IsKnownCollection<std::multimap<K, V, Compare, Alloc>> : std::true_type {};

template <typename K, typename V, typename Hash, typename Eq, typename Alloc>
struct IsKnownCollection<std::unordered_map<K, V, Hash, Eq, Alloc>> : std::true_type {};

template <typename K, typename V, typename Hash, typename Eq, typename Alloc>
struct IsKnownCollection<std::unordered_multimap<K, V, Hash, Eq, Alloc>> : std::true_type {};

template <typename... Ts>
struct IsKnownCollection<std::tuple<Ts...>> : std::true_type {};

template <typename K, typename V>
struct IsKnownCollection<std::pair<K, V>> : std::true_type {};

template <typename T>
inline constexpr bool is_known_collection_v = IsKnownCollection<std::remove_cvref_t<T>>::value; // NOLINT

template <typename T>
inline constexpr bool is_scalar_like_v = []() consteval { // NOLINT
    using value_type = unwrapped_optional_like_type_t<T>;
    return std::is_null_pointer_v<value_type> || std::is_arithmetic_v<value_type> || std::is_enum_v<value_type> ||
           is_string_like_v<value_type>;
}();

template <typename T>
inline constexpr bool is_collection_like_v = []() consteval { // NOLINT
    using value_type = unwrapped_optional_like_type_t<T>;
    if constexpr (is_known_collection_v<value_type>) {
        return true;
    } else if constexpr (is_scalar_like_v<value_type>) {
        return false;
    } else {
        return std::is_class_v<value_type>;
    }
}();

} // namespace traits
} // namespace nekoproto
