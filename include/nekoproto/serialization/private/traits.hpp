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

#include <optional>
#include <type_traits>
#include <variant>

NEKO_BEGIN_NAMESPACE
struct TestSerializableTrait {
    template <typename... T>
    bool operator()(T&&... /*unused*/) {
        return true;
    }
    bool saveValue(auto) { return true; }   // NOLINT
    bool loadValue(auto) { return true; }   // NOLINT
    bool startArray(auto) { return true; }  // NOLINT
    bool endArray() { return true; }        // NOLINT
    bool startObject(auto) { return true; } // NOLINT
    bool endObject() { return true; }       // NOLINT
    bool end() { return true; }             // NOLINT
    bool startNode() { return true; }       // NOLINT
    bool finishNode() { return true; }      // NOLINT
    void rollbackItem() {}                  // NOLINT
    bool isArray() { return true; }         // NOLINT
    bool isObject() { return true; }        // NOLINT
};
namespace traits {
namespace detail {
struct NameValuePairCore {}; // help to trait the type
struct SizeTagCore {};       // help to trait the type
} // namespace detail

template <typename T>
T& dereference(T* ptr) NEKO_NOEXCEPT {
    return *ptr;
}

class method_access { // NOLINT
public:
    template <typename SerializerT, typename T>
    static auto method_const_save(SerializerT& sa, const T& value) // NOLINT(readability-identifier-naming)
        NEKO_NOEXCEPT -> decltype(value.save(sa)) {
        return value.save(sa);
    }

    template <typename SerializerT, typename T>
    static auto method_save(SerializerT& sa, T& value) // NOLINT(readability-identifier-naming)
        NEKO_NOEXCEPT -> decltype(value.save(sa)) {
        return value.save(sa);
    }

    template <typename SerializerT, typename T>
    static auto method_load(SerializerT& sa, T& value) // NOLINT(readability-identifier-naming)
        NEKO_NOEXCEPT -> decltype(value.load(sa)) {
        return value.load(sa);
    }
};

template <class T, class A>
concept has_function_save = requires(A& sa, const T& value) {
    { save(sa, value) } -> std::convertible_to<bool>;
};

template <class T, class A>
concept has_function_load = requires(A& sa, T& value) {
    { load(sa, value) } -> std::convertible_to<bool>;
};

template <class T, class A>
concept has_method_save = requires(A& sa, const T& value) {
    { method_access::method_const_save(sa, value) };
} || requires(A& sa, const T& value) {
    { method_access::method_save(sa, value) };
};

template <class T, class A>
concept has_method_load = requires(A& sa, T& value) {
    { method_access::method_load(sa, value) };
};

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
struct optional_like_type : public std::false_type {};

template <typename T>
    requires optional_like<std::remove_cvref_t<T>>
struct optional_like_type<T, void> : public std::true_type {
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
struct optional_like_type<T, std::enable_if_t<has_monostate<std::remove_cvref_t<T>>::value>> : public std::true_type {
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
concept can_be_serializable =
    (has_function_save<T, TestSerializableTrait> && has_function_load<T, TestSerializableTrait>) ||
    (has_method_save<T, TestSerializableTrait> && has_method_load<T, TestSerializableTrait>);

template <typename T>
using ref_type = typename std::conditional<
    std::is_array<typename std::remove_reference<T>::type>::value, typename std::remove_cv<T>::type,
    typename std::conditional<std::is_lvalue_reference<T>::value, T, typename std::decay<T>::type>::type>::type;

} // namespace traits

template <typename T, class enable = void>
struct is_minimal_serializable : std::false_type {}; // NOLINT(readability-identifier-naming)

template <typename CharT, typename Traits, typename Alloc>
struct is_minimal_serializable<std::basic_string<CharT, Traits, Alloc>, void> : std::true_type {};

template <typename CharT, typename Traits>
struct is_minimal_serializable<std::basic_string_view<CharT, Traits>, void> : std::true_type {};

template <>
struct is_minimal_serializable<const char*, void> : std::true_type {};

template <typename T>
    requires std::is_arithmetic_v<T> || std::is_enum_v<T>
struct is_minimal_serializable<T, void> : std::true_type {};

template <typename T, class enable = void>
struct is_skipable : std::false_type {}; // NOLINT(readability-identifier-naming)
NEKO_END_NAMESPACE
