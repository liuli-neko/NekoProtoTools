/**
 * @file name_reflect.hpp
 * @author llhsdmd (llhsdmd@gmail.com)
 * @brief
 * @version 0.1
 * @date 2025-04-29
 *
 * @copyright Copyright (c) 2025
 *
 */
#pragma once

#include "global.hpp"
#include "string_literal.hpp"

#include <array>
#include <functional>
#include <optional>
#include <string_view>
#include <type_traits>

#ifdef __GNUC__
#include <cxxabi.h>
#include <tuple>
#endif

NEKO_BEGIN_NAMESPACE
namespace detail {
#if defined(__GNUC__) || defined(__MINGW__) || defined(__clang__)
#define NEKO_PRETTY_FUNCTION_NAME __PRETTY_FUNCTION__
#elif defined(_WIN32)
#define NEKO_PRETTY_FUNCTION_NAME __FUNCSIG__
#else
#define NEKO_PRETTY_FUNCTION_NAME __func__
#endif

#define MAX_UNWRAP_STRUCT_SIZE 128

template <std::size_t N, typename T>
    requires(N <= MAX_UNWRAP_STRUCT_SIZE)
constexpr auto unwrap_struct_impl(T& data) noexcept {
#define GENERATE_UNPACK_CASE(i)                                                                                        \
    else if constexpr (N == i) {                                                                                       \
        auto& [NEKO_PP_LIST_PARAMS(m, i)] = data;                                                                      \
        return std::forward_as_tuple(NEKO_PP_LIST_PARAMS(m, i));                                                       \
    }
    if constexpr (N == 0) {
        return std::forward_as_tuple();
    }
    NEKO_PP_REPEAT_MACRO(GENERATE_UNPACK_CASE, MAX_UNWRAP_STRUCT_SIZE)
#undef GENERATE_UNPACK_CASE
}

template <typename T, class enable = void>
struct is_optional : std::false_type {}; // NOLINT(readability-identifier-naming)

template <typename T>
struct is_optional<std::optional<T>, void> : std::true_type {};

template <typename T>
struct is_optional<std::optional<T>&, void> : std::true_type {};

template <typename T>
struct is_optional<const std::optional<T>, void> : std::true_type {};

template <typename T>
struct is_optional<const std::optional<T>&, void> : std::true_type {};

struct any_type { // NOLINT(readability-identifier-naming)
    template <typename T, typename = std::enable_if_t<!is_optional<std::decay_t<T>>::value>>
    operator T() {}
};
template <typename T, typename _Cond = void, typename... Args>
struct can_aggregate_impl : std::false_type {}; // NOLINT(readability-identifier-naming)
template <typename T, typename... Args>
struct can_aggregate_impl<T, std::void_t<decltype(T{std::declval<Args>()...})>, Args...> : std::true_type {};
template <typename T, typename... Args>
struct can_aggregate : can_aggregate_impl<T, void, Args...> {}; // NOLINT(readability-identifier-naming)

/**
 * @brief Get the struct size at compile time
 *
 * @tparam T
 * @return the size of the
 */
template <typename T, typename... Args>
constexpr auto member_count([[maybe_unused]] Args&&... args) noexcept {
    if constexpr ((!can_aggregate<T, Args..., any_type>::value) &&
                  (!can_aggregate<T, Args..., std::nullopt_t>::value)) {
        return sizeof...(args);
    } else if constexpr (can_aggregate<T, Args..., any_type>::value) {
        return member_count<T>(std::forward<Args>(args)..., any_type{});
    } else {
        return member_count<T>(std::forward<Args>(args)..., std::nullopt);
    }
}

template <typename T>
static constexpr size_t member_count_v = member_count<T>(); // NOLINT(readability-identifier-naming)

template <typename T>
struct is_std_array : std::false_type {}; // NOLINT(readability-identifier-naming)

template <typename T, size_t N>
struct is_std_array<std::array<T, N>> : std::true_type {};

template <typename T>
static constexpr bool can_unwrap_v = // NOLINT(readability-identifier-naming)
    std::is_aggregate_v<std::remove_cv_t<T>> && !is_std_array<T>::value;

/**
 * @brief Convert the struct reference to tuple
 *
 * @tparam T
 * @param data
 * @return constexpr auto
 */
template <typename T>
constexpr auto unwrap_struct(T& data) noexcept {
    static_assert(can_unwrap_v<T>, "The struct must be aggregate");
    static_assert(member_count_v<T> > 0, "The struct must have at least one member");
    static_assert(member_count_v<T> <= MAX_UNWRAP_STRUCT_SIZE, "The struct is too large");
    return unwrap_struct_impl<member_count_v<T>>(data);
}

template <class T>
inline static T external; // NOLINT(readability-identifier-naming)

template <class T>
struct ptr_t final { // NOLINT
    const T* ptr;
};

template <size_t N, class T>
constexpr auto get_ptr(T&& dt) noexcept {
    auto& val = get<N>(unwrap_struct(dt));
    return ptr_t<std::remove_cvref_t<decltype(val)>>{&val};
}

template <auto Ptr>
[[nodiscard]] consteval auto mangled_name() {
    return NEKO_PRETTY_FUNCTION_NAME;
}

template <class T>
[[nodiscard]] consteval auto mangled_name() {
    return NEKO_PRETTY_FUNCTION_NAME;
}

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
template <auto N, class T>
constexpr std::string_view get_name_impl = // NOLINT(readability-identifier-naming)
    mangled_name<get_ptr<N>(external<std::remove_volatile_t<T>>)>();
#pragma clang diagnostic pop
#elif __GNUC__
template <auto N, class T>
constexpr std::string_view get_name_impl = mangled_name<get_ptr<N>(external<std::remove_volatile_t<T>>)>();
#else
template <auto N, class T>
constexpr std::string_view get_name_impl = mangled_name<get_ptr<N>(external<std::remove_volatile_t<T>>)>();
#endif

struct NekoReflector {
    int nekoField;
    static void nekoStaticFunc() {}
    void nekoFunc() {}
};

struct reflect_type {                                                       // NOLINT(readability-identifier-naming)
    static constexpr std::string_view name = mangled_name<NekoReflector>(); // NOLINT(readability-identifier-naming)
    static constexpr auto end = name.substr(name.find("NekoReflector") + sizeof("NekoReflector") - 1); // NOLINT
#if defined(__GNUC__) || defined(__clang__)
    static constexpr auto begin = std::string_view{"T = "}; // NOLINT
#else
    static constexpr auto begin = std::string_view{"mangled_name<"};
#endif
};

struct reflect_field {                                                                           // NOLINT
    static constexpr auto name  = get_name_impl<0, NekoReflector>;                               // NOLINT
    static constexpr auto end   = name.substr(name.find("nekoField") + sizeof("nekoField") - 1); // NOLINT
    static constexpr auto begin = name[name.find("nekoField") - 1];                              // NOLINT
};

template <std::size_t N, class T>
struct member_nameof_impl {                                                                  // NOLINT
    static constexpr auto name     = get_name_impl<N, T>;                                    // NOLINT
    static constexpr auto begin    = name.find(reflect_field::end);                          // NOLINT
    static constexpr auto tmp      = name.substr(0, begin);                                  // NOLINT
    static constexpr auto stripped = tmp.substr(tmp.find_last_of(reflect_field::begin) + 1); // NOLINT

    static constexpr std::string_view stripped_literal = join_v<stripped>; // NOLINT
};

template <const std::string_view& str>
constexpr std::string_view parser_class_name_with_type() {
    std::string_view string;
    string = str.find_last_of(' ') == std::string_view::npos ? str : str.substr(str.find_last_of(' ') + 1);
    string = string.find_last_of(':') == std::string_view::npos ? string : string.substr(string.find_last_of(':') + 1);
    return string;
}

template <class T>
struct class_nameof_impl {                                                     // NOLINT
    static constexpr std::string_view name     = mangled_name<T>();            // NOLINT
    static constexpr auto begin                = name.find(reflect_type::end); // NOLINT
    static constexpr auto tmp                  = name.substr(0, begin);        // NOLINT
    static constexpr auto class_name_with_type =                               // NOLINT
        tmp.substr(tmp.find(reflect_type::begin) + reflect_type::begin.size());
    static constexpr auto stripped = // NOLINT
        parser_class_name_with_type<class_name_with_type>();

    static constexpr std::string_view stripped_literal = join_v<stripped>; // NOLINT
};

template <std::size_t N, class T>
inline constexpr auto member_nameof = []() constexpr { return member_nameof_impl<N, T>::stripped_literal; }(); // NOLINT

template <class T>
inline constexpr auto class_nameof = []() constexpr { return class_nameof_impl<T>::stripped_literal; }(); // NOLINT

template <class T, std::size_t... Is>
[[nodiscard]] constexpr auto member_names_impl(std::index_sequence<Is...> /*unused*/) {
    if constexpr (sizeof...(Is) == 0) {
        return std::array<std::string_view, 0>{};
    } else {
        return std::array{member_nameof<Is, T>...};
    }
}

/// ====================== enum string =========================
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wenum-constexpr-conversion"
#endif
#ifndef NEKO_ENUM_SEARCH_DEPTH
#define NEKO_ENUM_SEARCH_DEPTH 60
#endif
#if defined(__GNUC__) || defined(__MINGW__) || defined(__clang__)
template <typename T, T Value>
constexpr auto neko_get_enum_name() noexcept {
    // constexpr auto _Neko_GetEnumName() [with T = MyEnum; T Value = MyValues]
    // constexpr auto _Neko_GetEnumName() [with T = MyEnum; T Value =
    // (MyEnum)114514]"
    std::string_view name(__PRETTY_FUNCTION__);
    std::size_t eqBegin   = name.find_last_of(' ');
    std::size_t end       = name.find_last_of(']');
    std::string_view body = name.substr(eqBegin + 1, end - eqBegin - 1);
    if (body[0] == '(') {
        // Failed
        return std::string_view();
    }
    return body;
}
#elif defined(_MSC_VER)
#define NEKO_ENUM_TO_NAME(enumType)
template <typename T, T Value>
constexpr auto neko_get_enum_name() noexcept {
    // auto __cdecl _Neko_GetEnumName<enum main::MyEnum,(enum
    // main::MyEnum)0x2>(void) auto __cdecl _Neko_GetEnumName<enum
    // main::MyEnum,main::MyEnum::Wtf>(void)
    std::string_view name(__FUNCSIG__);
    std::size_t dotBegin  = name.find_first_of(',');
    std::size_t end       = name.find_last_of('>');
    std::string_view body = name.substr(dotBegin + 1, end - dotBegin - 1);
    if (body[0] == '(') {
        // Failed
        return std::string_view();
    }
    return body;
}
#else
template <typename T, T Value>
constexpr auto neko_get_enum_name() noexcept {
    // Unsupported
    return std::string_view();
}
#endif
template <typename T, T Value>
constexpr bool neko_is_valid_enum() noexcept {
    return !neko_get_enum_name<T, Value>().empty();
}
template <typename T, std::size_t... N>
constexpr std::size_t neko_get_valid_enum_count(std::index_sequence<N...> /*unused*/) noexcept {
    return (... + neko_is_valid_enum<T, T(N)>());
}
template <typename T, std::size_t... N>
constexpr auto neko_get_valid_enum_names(std::index_sequence<N...> seq) noexcept {
    constexpr auto ValidCount = neko_get_valid_enum_count<T>(seq);

    std::array<std::pair<T, std::string_view>, ValidCount> arr;
    std::string_view vstr[sizeof...(N)]{neko_get_enum_name<T, T(N)>()...};

    std::size_t ns   = 0;
    std::size_t left = ValidCount;
    auto iter        = arr.begin();

    for (auto idx : vstr) {
        if (!idx.empty()) {
            // Valid name
            iter->first  = T(ns);
            iter->second = idx;
            ++iter;
        }
        if (left == 0) {
            break;
        }

        ns += 1;
    }
    return arr;
}

// MARK: function traits
template <typename T, class Enable = void>
struct function_traits; // 主模板

// 特化：普通函数指针
template <typename R, typename... Args>
struct function_traits<R (*)(Args...), void> {
    using return_type = R;
    using arg_tuple   = std::tuple<Args...>;
    template <typename Ret, template <typename...> class T>
    using args_in                                = T<Ret, Args...>;
    static constexpr auto is_member_func         = false; // 是否为成员函数指针
    static constexpr std::string_view class_name = "";
};

// 特化：普通函数类型
template <typename R, typename... Args>
struct function_traits<R(Args...), void> : function_traits<R (*)(Args...)> {};

// 特化：std::function
template <typename R, typename... Args>
struct function_traits<std::function<R(Args...)>, void> : function_traits<R (*)(Args...)> {};

#if __cpp_lib_move_only_function >= 202110L
// 特化：std::move_only_function
template <typename R, typename... Args>
struct function_traits<std::move_only_function<R(Args...)>, void> : function_traits<R (*)(Args...)> {};
#endif

// 特化：成员函数指针
template <typename C, typename R, typename... Args>
struct function_traits<R (C::*)(Args...), void> : function_traits<R (*)(Args...)> {
    static constexpr auto is_member_func         = true; // 是否为成员函数指针
    static constexpr std::string_view class_name = class_nameof<C>;
};

template <typename Functor>
struct function_traits<Functor, std::void_t<decltype(&std::remove_cvref_t<Functor>::operator())>>
    : function_traits<decltype(&std::remove_cvref_t<Functor>::operator())> {};

#define MEMBER_FUNCTION_TRAITS_QUALIFIER(QUAL)                                                                         \
    template <typename C, typename R, typename... Args>                                                                \
    struct function_traits<R (C::*)(Args...) QUAL> : function_traits<R (C::*)(Args...)> {};

MEMBER_FUNCTION_TRAITS_QUALIFIER(const)
MEMBER_FUNCTION_TRAITS_QUALIFIER(volatile)
MEMBER_FUNCTION_TRAITS_QUALIFIER(const volatile)
MEMBER_FUNCTION_TRAITS_QUALIFIER(&)
MEMBER_FUNCTION_TRAITS_QUALIFIER(const&)
MEMBER_FUNCTION_TRAITS_QUALIFIER(volatile&)
MEMBER_FUNCTION_TRAITS_QUALIFIER(const volatile&)
MEMBER_FUNCTION_TRAITS_QUALIFIER(&&)
MEMBER_FUNCTION_TRAITS_QUALIFIER(const&&)
MEMBER_FUNCTION_TRAITS_QUALIFIER(volatile&&)
MEMBER_FUNCTION_TRAITS_QUALIFIER(const volatile&&)
#if __cpp_noexcept_function_type
MEMBER_FUNCTION_TRAITS_QUALIFIER(noexcept)
MEMBER_FUNCTION_TRAITS_QUALIFIER(const noexcept)
MEMBER_FUNCTION_TRAITS_QUALIFIER(volatile noexcept)
MEMBER_FUNCTION_TRAITS_QUALIFIER(const volatile noexcept)
MEMBER_FUNCTION_TRAITS_QUALIFIER(& noexcept)
MEMBER_FUNCTION_TRAITS_QUALIFIER(const& noexcept)
MEMBER_FUNCTION_TRAITS_QUALIFIER(volatile& noexcept)
MEMBER_FUNCTION_TRAITS_QUALIFIER(const volatile& noexcept)
MEMBER_FUNCTION_TRAITS_QUALIFIER(&& noexcept)
MEMBER_FUNCTION_TRAITS_QUALIFIER(const&& noexcept)
MEMBER_FUNCTION_TRAITS_QUALIFIER(volatile&& noexcept)
MEMBER_FUNCTION_TRAITS_QUALIFIER(const volatile&& noexcept)
#endif
#undef MEMBER_FUNCTION_TRAITS_QUALIFIER

template <typename T>
    requires requires(T tt) {
        { tt.operator()() };
    }
struct function_traits<T> : function_traits<decltype(&std::remove_cvref_t<T>::operator())> {};

/**
 * @brief reflect member function helper
 * in linux
 * consteval auto NekoProto::detail::mangled_name() [with auto Ptr = NekoReflector::nekoStaticFunc]
 * consteval auto NekoProto::detail::mangled_name() [with auto Ptr = test_func_with_struct]
 * in windows
 * auto __cdecl NekoProto::detail::mangled_name<void __cdecl
 * NekoProto::detail::NekoReflector::nekoStaticFunc(void)>(void)
 * auto __cdecl NekoProto::detail::mangled_name<int __cdecl free_func_one_arg(int)>(void)
 */
struct reflect_static_func {                                                                                // NOLINT
    static constexpr std::string_view name                 = mangled_name<NekoReflector::nekoStaticFunc>(); // NOLINT
    static constexpr std::string_view characteristicString = "NekoReflector::nekoStaticFunc";
    static constexpr auto end                              = // NOLINT
        name.substr(name.find(characteristicString) + characteristicString.size());
    static constexpr auto begin = name[name.find(characteristicString) - 1]; // NOLINT
};

template <auto Ptr>
    requires(std::is_member_function_pointer_v<decltype(Ptr)> || std::is_pointer_v<decltype(Ptr)>)
struct func_nameof_impl {                                         // NOLINT
    static constexpr std::string_view name = mangled_name<Ptr>(); // NOLINT
#if defined(__clang__) || defined(__GNUC__)
    static constexpr auto begin          = name.find(reflect_static_func::end);                          // NOLINT
    static constexpr auto tmp            = name.substr(0, begin);                                        // NOLINT
    static constexpr auto stripped       = tmp.substr(tmp.find_last_of(reflect_static_func::begin) + 1); // NOLINT
    static constexpr auto seq            = stripped.find_last_of("::");                                  // NOLINT
    static constexpr auto is_member_func = seq != std::string_view::npos;                                // NOLINT
    static constexpr auto func_name      = is_member_func ? stripped.substr(seq + 1) : stripped;         // NOLINT
#elif defined(_MSC_VER)
    static constexpr std::string_view characteristicString = "auto __cdecl NekoProto::detail::mangled_name<"; // NOLINT
    // void __cdecl NekoProto::detail::NekoReflector::nekoStaticFunc(void)>(void)
    // int __cdecl free_func_one_arg(int)>(void)
    static constexpr auto full_function_name =                                                   // NOLINT
        name.substr(characteristicString.size(), name.size() - characteristicString.size() - 6); // NOLINT
    static constexpr auto end    = full_function_name.find_first_of('(');                        // NOLINT
    static constexpr auto begin1 = full_function_name.substr(0, end).find_last_of(' ');          // NOLINT
    static constexpr auto begin2 = full_function_name.substr(0, end).find_last_of("::");         // NOLINT
    static constexpr auto begin  = begin2 != std::string_view::npos ? begin2 + 1 : begin1 + 1;   // NOLINT
    static_assert(begin2 != std::string_view::npos || begin1 != std::string_view::npos, "function begin not found");
    static_assert(end != std::string_view::npos, "function end not found");
    static constexpr auto tmp                   = full_function_name.substr(begin, end - begin); // NOLINT
    static constexpr std::string_view func_name = join_v<tmp>;                                   // NOLINT
#else
    static_assert(false, "unsupported compiler");
#endif
};

template <auto Ptr>
inline constexpr auto func_nameof = []() constexpr { return func_nameof_impl<Ptr>::func_name; }(); // NOLINT

#ifdef __clang__
#pragma clang diagnostic pop
#endif
} // namespace detail
NEKO_END_NAMESPACE