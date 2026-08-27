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

#include "detail/unwrap_struct.hpp"
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

namespace nekoproto {
namespace detail {
#if defined(__GNUC__) || defined(__MINGW__) || defined(__clang__)
#define NEKO_PRETTY_FUNCTION_NAME __PRETTY_FUNCTION__
#elif defined(_WIN32)
#define NEKO_PRETTY_FUNCTION_NAME __FUNCSIG__
#else
#define NEKO_PRETTY_FUNCTION_NAME __func__
#endif

template <typename T, class enable = void>
struct IsOptional : std::false_type {};

template <typename T>
struct IsOptional<std::optional<T>, void> : std::true_type {};

template <typename T>
struct IsOptional<std::optional<T>&, void> : std::true_type {};

template <typename T>
struct IsOptional<const std::optional<T>, void> : std::true_type {};

template <typename T>
struct IsOptional<const std::optional<T>&, void> : std::true_type {};

struct AnyType {
    template <typename T, typename = std::enable_if_t<!IsOptional<std::decay_t<T>>::value>>
    operator T() const noexcept {}
};
template <typename T, typename _Cond = void, typename... Args>
struct CanAggregateImpl : std::false_type {};
template <typename T, typename... Args>
struct CanAggregateImpl<T, std::void_t<decltype(T{std::declval<Args>()...})>, Args...> : std::true_type {};
template <typename T, typename... Args>
struct CanAggregate : CanAggregateImpl<T, void, Args...> {};

/**
 * @brief Get the struct size at compile time
 *
 * @tparam T
 * @return the size of the
 */
template <typename T, typename... Args>
constexpr auto memberCount([[maybe_unused]] Args&&... args) noexcept {
    if constexpr ((!CanAggregate<T, Args..., AnyType>::value) && (!CanAggregate<T, Args..., std::nullopt_t>::value)) {
        return sizeof...(args);
    } else if constexpr (CanAggregate<T, Args..., AnyType>::value) {
        return memberCount<T>(std::forward<Args>(args)..., AnyType{});
    } else {
        return memberCount<T>(std::forward<Args>(args)..., std::nullopt);
    }
}

template <typename T>
static constexpr size_t member_count_v = memberCount<T>();

template <typename T>
struct IsStdArray : std::false_type {};

template <typename T, size_t N>
struct IsStdArray<std::array<T, N>> : std::true_type {
    using value_type             = T;
    constexpr static size_t size = N;
};

template <typename T>
static constexpr bool can_unwrap_v = std::is_aggregate_v<std::remove_cv_t<T>> && !IsStdArray<T>::value;

/**
 * @brief Convert the struct reference to tuple
 *
 * @tparam T
 * @param data
 * @return constexpr auto
 */
template <typename T>
constexpr auto unwrapStruct(T& data) noexcept {
    static_assert(can_unwrap_v<T>, "The struct must be aggregate");
    static_assert(member_count_v<T> > 0, "The struct must have at least one member");
    static_assert(member_count_v<T> <= max_unwrap_struct_size, "The struct is too large");
    return unwrapStructImpl<member_count_v<T>>(data);
}

template <class T>
inline static T s_external;

template <class T>
struct PtrT final {
    const T* ptr;
};

template <size_t N, class T>
constexpr auto getPtr(T&& dt) noexcept {
    auto& val = get<N>(unwrapStruct(dt));
    return PtrT<std::remove_cvref_t<decltype(val)>>{&val};
}

template <auto Ptr>
[[nodiscard]] consteval auto mangledName() {
    return NEKO_PRETTY_FUNCTION_NAME;
}

template <class T>
[[nodiscard]] consteval auto mangledName() {
    return NEKO_PRETTY_FUNCTION_NAME;
}

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
template <auto N, class T>
constexpr std::string_view get_name_impl = mangledName<getPtr<N>(s_external<std::remove_volatile_t<T>>)>();
#pragma clang diagnostic pop
#elif __GNUC__
template <auto N, class T>
constexpr std::string_view get_name_impl = mangledName<getPtr<N>(s_external<std::remove_volatile_t<T>>)>();
#else
template <auto N, class T>
constexpr std::string_view get_name_impl = mangledName<getPtr<N>(s_external<std::remove_volatile_t<T>>)>();
#endif

struct NekoReflector {
    int nekoField;
    static void nekoStaticFunc() {}
    void nekoFunc() {}
};

struct ReflectType {
    static constexpr std::string_view name = mangledName<NekoReflector>();
    static constexpr auto end              = name.substr(name.find("NekoReflector") + sizeof("NekoReflector") - 1);
#if defined(__GNUC__) || defined(__clang__)
    static constexpr auto begin = std::string_view{"T = "};
#else
    static constexpr auto begin = std::string_view{"mangledName<"};
#endif
};

struct ReflectField {
    static constexpr auto name  = get_name_impl<0, NekoReflector>;
    static constexpr auto end   = name.substr(name.find("nekoField") + sizeof("nekoField") - 1);
    static constexpr auto begin = name[name.find("nekoField") - 1];
};

template <std::size_t N, class T>
struct MemberNameofImpl {
    static constexpr auto name     = get_name_impl<N, T>;
    static constexpr auto begin    = name.find(ReflectField::end);
    static constexpr auto tmp      = name.substr(0, begin);
    static constexpr auto stripped = tmp.substr(tmp.find_last_of(ReflectField::begin) + 1);

    static constexpr std::string_view stripped_literal = join_v<stripped>;
};

template <const std::string_view& str>
constexpr auto parserClassNameWithType() -> std::string_view {
    std::string_view string;
    string = str.find_last_of(' ') == std::string_view::npos ? str : str.substr(str.find_last_of(' ') + 1);
    string = string.find_last_of(':') == std::string_view::npos ? string : string.substr(string.find_last_of(':') + 1);
    return string;
}

template <class T>
struct ClassNameofImpl {
    static constexpr std::string_view name     = mangledName<T>();
    static constexpr auto begin                = name.find(ReflectType::end);
    static constexpr auto tmp                  = name.substr(0, begin);
    static constexpr auto class_name_with_type = tmp.substr(tmp.find(ReflectType::begin) + ReflectType::begin.size());
    static constexpr auto stripped             = parserClassNameWithType<class_name_with_type>();

    static constexpr std::string_view stripped_literal = join_v<stripped>;
};

template <std::size_t N, class T>
inline constexpr auto member_nameof = []() constexpr { return MemberNameofImpl<N, T>::stripped_literal; }();

template <class T>
inline constexpr auto class_nameof = []() constexpr { return ClassNameofImpl<T>::stripped_literal; }();

template <class T, std::size_t... Is>
[[nodiscard]] constexpr auto memberNamesImpl(std::index_sequence<Is...> /*unused*/) {
    if constexpr (sizeof...(Is) == 0) {
        return std::array<std::string_view, 0>{};
    } else {
        return std::array{member_nameof<Is, T>...};
    }
}

/// ====================== enum string =========================
#ifdef __clang__
#pragma clang diagnostic push
// #pragma clang diagnostic ignored "-Wenum-constexpr-conversion"
#endif
#ifndef NEKO_ENUM_SEARCH_DEPTH
#define NEKO_ENUM_SEARCH_DEPTH 60
#endif
#if defined(__GNUC__) || defined(__MINGW__) || defined(__clang__)
template <typename T, T Value>
constexpr auto nekoGetEnumName() noexcept {
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
template <typename T, T Value>
constexpr auto nekoGetEnumName() noexcept {
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
constexpr auto nekoGetEnumName() noexcept {
    // Unsupported
    return std::string_view();
}
#endif
template <typename T, T Value>
constexpr auto nekoIsValidEnum() noexcept -> bool {
    return !nekoGetEnumName<T, Value>().empty();
}
template <typename T, std::size_t... N>
constexpr auto nekoGetValidEnumCount(std::index_sequence<N...> /*unused*/) noexcept -> std::size_t {
    return (... + nekoIsValidEnum<T, T(N)>());
}
template <typename T, std::size_t... N>
constexpr auto nekoGetValidEnumNames(std::index_sequence<N...> seq) noexcept {
    constexpr auto ValidCount = nekoGetValidEnumCount<T>(seq);

    std::array<std::pair<T, std::string_view>, ValidCount> arr;
    std::string_view vstr[sizeof...(N)]{nekoGetEnumName<T, T(N)>()...};

    std::size_t ns   = 0;
    std::size_t left = ValidCount;
    auto iter        = arr.begin();

    for (auto idx : vstr) {
        if (!idx.empty()) {
            // Valid name
            iter->first = T(ns);
            auto pos    = idx.find_last_of(':');
            if (pos != std::string_view::npos) {
                iter->second = idx.substr(pos + 1);
            } else {
                iter->second = idx;
            }
            ++iter;
        }
        if (left == 0) {
            break;
        }

        ns += 1;
    }
    return arr;
}

// One canonical automatic-reflection result per enum. The expensive search is
// shared by names(), values(), maps, and enum metadata traversal instead of
// being reevaluated independently by every consumer.
template <typename T>
struct EnumReflectionTable {
    static_assert(std::is_enum_v<T>);

    static constexpr auto entries     = nekoGetValidEnumNames<T>(std::make_index_sequence<NEKO_ENUM_SEARCH_DEPTH>{});
    static constexpr std::size_t size = entries.size();
    static constexpr auto names       = [] {
        std::array<std::string_view, size> result{};
        for (std::size_t i = 0; i < size; ++i) {
            result[i] = entries[i].second;
        }
        return result;
    }();
    static constexpr auto values = [] {
        std::array<T, size> result{};
        for (std::size_t i = 0; i < size; ++i) {
            result[i] = entries[i].first;
        }
        return result;
    }();
};

// MARK: function traits
template <typename T, class Enable = void>
struct FunctionTraits; // 主模板

// 特化：普通函数指针
template <typename R, typename... Args>
struct FunctionTraits<R (*)(Args...), void> {
    using return_type = R;
    using arg_tuple   = std::tuple<Args...>;
    template <typename Ret, template <typename...> class T>
    using args_in               = T<Ret, Args...>;
    using function_type         = R(Args...);
    using function_pointer_type = R (*)(Args...);
};

// 特化：普通函数类型
template <typename R, typename... Args>
struct FunctionTraits<R(Args...), void> : FunctionTraits<R (*)(Args...)> {};

// 特化：std::function
template <typename R, typename... Args>
struct FunctionTraits<std::function<R(Args...)>, void> : FunctionTraits<R (*)(Args...)> {};

#if __cpp_lib_move_only_function >= 202110L
// 特化：std::move_only_function
template <typename R, typename... Args>
struct FunctionTraits<std::move_only_function<R(Args...)>, void> : FunctionTraits<R (*)(Args...)> {};
#endif

// 特化：成员函数指针
template <typename C, typename R, typename... Args>
struct FunctionTraits<R (C::*)(Args...), void> : FunctionTraits<R (*)(Args...)> {};

template <typename Functor>
struct FunctionTraits<Functor, std::void_t<decltype(&std::remove_cvref_t<Functor>::operator())>>
    : FunctionTraits<decltype(&std::remove_cvref_t<Functor>::operator())> {};

#define NEKO_DETAIL_MEMBER_FUNCTION_TRAITS_QUALIFIER(QUAL)                                                             \
    template <typename C, typename R, typename... Args>                                                                \
    struct FunctionTraits<R (C::*)(Args...) QUAL> : FunctionTraits<R (C::*)(Args...)> {};

NEKO_DETAIL_MEMBER_FUNCTION_TRAITS_QUALIFIER(const)
NEKO_DETAIL_MEMBER_FUNCTION_TRAITS_QUALIFIER(volatile)
NEKO_DETAIL_MEMBER_FUNCTION_TRAITS_QUALIFIER(const volatile)
NEKO_DETAIL_MEMBER_FUNCTION_TRAITS_QUALIFIER(&)
NEKO_DETAIL_MEMBER_FUNCTION_TRAITS_QUALIFIER(const&)
NEKO_DETAIL_MEMBER_FUNCTION_TRAITS_QUALIFIER(volatile&)
NEKO_DETAIL_MEMBER_FUNCTION_TRAITS_QUALIFIER(const volatile&)
NEKO_DETAIL_MEMBER_FUNCTION_TRAITS_QUALIFIER(&&)
NEKO_DETAIL_MEMBER_FUNCTION_TRAITS_QUALIFIER(const&&)
NEKO_DETAIL_MEMBER_FUNCTION_TRAITS_QUALIFIER(volatile&&)
NEKO_DETAIL_MEMBER_FUNCTION_TRAITS_QUALIFIER(const volatile&&)
#if __cpp_noexcept_function_type
NEKO_DETAIL_MEMBER_FUNCTION_TRAITS_QUALIFIER(noexcept)
NEKO_DETAIL_MEMBER_FUNCTION_TRAITS_QUALIFIER(const noexcept)
NEKO_DETAIL_MEMBER_FUNCTION_TRAITS_QUALIFIER(volatile noexcept)
NEKO_DETAIL_MEMBER_FUNCTION_TRAITS_QUALIFIER(const volatile noexcept)
NEKO_DETAIL_MEMBER_FUNCTION_TRAITS_QUALIFIER(& noexcept)
NEKO_DETAIL_MEMBER_FUNCTION_TRAITS_QUALIFIER(const& noexcept)
NEKO_DETAIL_MEMBER_FUNCTION_TRAITS_QUALIFIER(volatile& noexcept)
NEKO_DETAIL_MEMBER_FUNCTION_TRAITS_QUALIFIER(const volatile& noexcept)
NEKO_DETAIL_MEMBER_FUNCTION_TRAITS_QUALIFIER(&& noexcept)
NEKO_DETAIL_MEMBER_FUNCTION_TRAITS_QUALIFIER(const&& noexcept)
NEKO_DETAIL_MEMBER_FUNCTION_TRAITS_QUALIFIER(volatile&& noexcept)
NEKO_DETAIL_MEMBER_FUNCTION_TRAITS_QUALIFIER(const volatile&& noexcept)
#endif
#undef NEKO_DETAIL_MEMBER_FUNCTION_TRAITS_QUALIFIER

template <typename T>
    requires requires(T tt) {
        { tt.operator()() };
    }
struct FunctionTraits<T> : FunctionTraits<decltype(&std::remove_cvref_t<T>::operator())> {};

/**
 * @brief reflect function name
 * in linux
 * consteval auto nekoproto::detail::mangledName() [with auto Ptr = NekoReflector::nekoStaticFunc]
 * consteval auto nekoproto::detail::mangledName() [with auto Ptr = testFuncWithStruct]
 * in windows
 * auto __cdecl nekoproto::detail::mangledName<void __cdecl
 * nekoproto::detail::NekoReflector::nekoStaticFunc(void)>(void)
 * auto __cdecl nekoproto::detail::mangledName<int __cdecl freeFuncOneArg(int)>(void)
 * in clang
 * auto nekoproto::detail::mangledName() [Ptr = &nekoproto::detail::NekoReflector::nekoStaticFunc]
 * auto nekoproto::detail::mangledName() [Ptr = &test_func]
 */
template <auto Ptr>
    requires(std::is_pointer_v<decltype(Ptr)>)
struct FuncNameofImpl {
    static constexpr std::string_view name = mangledName<Ptr>();
#if defined(__clang__)
    // auto nekoproto::detail::mangledName() [Ptr = &nekoproto::detail::NekoReflector::nekoStaticFunc]
    // auto nekoproto::detail::mangledName() [Ptr = &test_func]
    static constexpr std::string_view characteristic_string = "auto nekoproto::detail::mangledName() [Ptr = &";
    static constexpr auto full_function_name =
        name.substr(characteristic_string.size(), name.size() - characteristic_string.size() - 1);
    static constexpr auto begin = full_function_name.find_last_of("::");
    static constexpr auto func_name =
        begin == std::string_view::npos ? full_function_name : full_function_name.substr(begin + 1);
#elif defined(__GNUC__)
    static constexpr std::string_view characteristicString =
        "consteval auto nekoproto::detail::mangledName() [with auto Ptr = ";
    static constexpr auto full_function_name =
        name.substr(characteristicString.size(), name.size() - characteristicString.size() - 1);
    static_assert(full_function_name.size() > 0, "can not find a valid function name");
    static constexpr auto seq            = full_function_name.find_last_of("::");
    static constexpr auto is_member_func = seq != std::string_view::npos;
    static constexpr auto func_name      = is_member_func ? full_function_name.substr(seq + 1) : full_function_name;
#elif defined(_MSC_VER)
    static constexpr std::string_view characteristicString = "auto __cdecl nekoproto::detail::mangledName<";
    // void __cdecl nekoproto::detail::NekoReflector::nekoStaticFunc(void)>(void)
    // int __cdecl freeFuncOneArg(int)>(void)
    static constexpr auto full_function_name =
        name.substr(characteristicString.size(), name.size() - characteristicString.size() - 6);
    static constexpr auto end = full_function_name.find_last_of('(');
    static_assert(end != std::string_view::npos, "function end not found");
    static constexpr auto before_params      = full_function_name.substr(0, end);
    static constexpr std::string_view xcdecl = "__cdecl ";
    static constexpr auto cdecl_pos          = before_params.find(xcdecl);
    static constexpr auto name_begin =
        cdecl_pos == std::string_view::npos ? before_params.find_last_of(' ') + 1 : cdecl_pos + xcdecl.size();
    static_assert(name_begin != std::string_view::npos, "function begin not found");
    static constexpr auto qualified_name    = before_params.substr(name_begin);
    static constexpr std::string_view scope = "::";
    static constexpr auto scope_pos         = qualified_name.rfind(scope);
    static constexpr auto tmp =
        scope_pos == std::string_view::npos ? qualified_name : qualified_name.substr(scope_pos + scope.size());
    static constexpr std::string_view func_name = join_v<tmp>;
#else
    static_assert(false, "unsupported compiler");
#endif
};

template <auto Ptr>
inline constexpr auto func_nameof = []() constexpr { return FuncNameofImpl<Ptr>::func_name; }();

template <auto MemberPtr>
    requires std::is_member_object_pointer_v<decltype(MemberPtr)>
consteval auto memberPointerName() -> std::string_view {
    std::string_view name = mangledName<MemberPtr>();
#if defined(__clang__) || defined(__GNUC__)
    auto start = name.find("&");
    if (start == std::string_view::npos) {
        return {};
    }
    auto end   = name.find(']', start);
    auto full  = name.substr(start + 1, end - start - 1);
    auto scope = full.rfind("::");
    return scope == std::string_view::npos ? full : full.substr(scope + 2);
#elif defined(_MSC_VER)
    auto start = name.find("&");
    if (start == std::string_view::npos) {
        return {};
    }
    auto end   = name.find('>', start);
    auto full  = name.substr(start + 1, end - start - 1);
    auto scope = full.rfind("::");
    return scope == std::string_view::npos ? full : full.substr(scope + 2);
#else
    return {};
#endif
}

#ifdef __clang__
#pragma clang diagnostic pop
#endif
} // namespace detail
} // namespace nekoproto
