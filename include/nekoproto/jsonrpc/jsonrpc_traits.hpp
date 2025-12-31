/**
 * @file jsonrpc_traits.hpp
 * @author llhsdmd(llhsdmd@gmail.com)
 * @brief
 * @version 0.1
 * @date 2025-04-17
 *
 * @copyright Copyright (c) 2025
 *
 */
#pragma once

#include <concepts>
#include <ilias/task.hpp>
#include <string_view>
#include <type_traits>

#include "nekoproto/global/reflect.hpp"
#include "nekoproto/jsonrpc/jsonrpc_error.hpp"
#include "nekoproto/serialization/json_serializer.hpp"
#include "nekoproto/serialization/serializer_base.hpp"
#include "nekoproto/serialization/types/types.hpp"
#include "nekoproto/serialization/types/unordered_map.hpp"

NEKO_BEGIN_NAMESPACE
namespace traits {
// MARK: serialization traits
template <typename T>
concept Serializable =
    (traits::has_method_save<std::decay_t<T>, JsonSerializer::OutputSerializer> ||
     traits::has_function_save<std::decay_t<T>, JsonSerializer::OutputSerializer> || std::is_same_v<T, const char*>) &&
    (traits::has_method_load<std::decay_t<T>, JsonSerializer::OutputSerializer> ||
     traits::has_function_load<std::decay_t<T>, JsonSerializer::OutputSerializer> || std::is_same_v<T, const char*>);
template <typename T, class enable = void>
struct IsSerializable : std::false_type {};
template <Serializable T>
struct IsSerializable<T, void> : std::true_type {};
template <>
struct IsSerializable<void> : std::true_type {};

template <typename T, class enable = void>
struct TypeName {
    static std::string name() { return std::string(NEKO_NAMESPACE::detail::class_nameof<T>); }
};

template <ConstexprString... ArgNames>
struct ArgNamesHelper {};

template <typename RawParamsType, std::size_t... Is, ConstexprString... ArgNames>
constexpr auto parameter_to_string(std::index_sequence<Is...> /*unused*/, ArgNamesHelper<ArgNames...> /*unused*/ = {})
    -> std::string {
    if constexpr (sizeof...(Is) == 0) {
        return "";
    } else if constexpr (sizeof...(ArgNames) != sizeof...(Is)) {
        constexpr std::size_t Nt = sizeof...(Is) - 1;
        return ((traits::TypeName<std::tuple_element_t<Is, RawParamsType>>::name() + (Is == Nt ? "" : ", ")) + ...);
    } else {
        constexpr std::size_t Nt                                  = sizeof...(Is) - 1;
        std::array<std::string_view, sizeof...(Is)> argNamesArray = {ArgNames.view()...};
        return ((traits::TypeName<std::tuple_element_t<Is, RawParamsType>>::name() + " " +
                 std::string(argNamesArray[Is]) + (Is == Nt ? "" : ", ")) +
                ...);
    }
};
template <>
struct TypeName<void, void> {
    static std::string name() { return "void"; }
};
template <typename T>
struct TypeName<std::optional<T>, void> {
    static std::string name() { return "std::optional<" + TypeName<T>::name() + ">"; }
};
template <typename... Args>
struct TypeName<std::variant<Args...>, void> {
    static std::string name() {
        return "std::variant<" + parameter_to_string<std::tuple<Args...>>(std::make_index_sequence<sizeof...(Args)>{}) +
               ">";
    }
};
template <typename T, std::size_t N>
struct TypeName<std::array<T, N>, void> {
    static std::string name() { return "std::array<" + TypeName<T>::name() + ", " + std::to_string(N) + ">"; }
};
template <typename... Args>
struct TypeName<std::tuple<Args...>, void> {
    ;
    static std::string name() {
        return "std::tuple<" + parameter_to_string<std::tuple<Args...>>(std::make_index_sequence<sizeof...(Args)>{}) +
               ">";
    }
};
template <typename T, typename T2>
struct TypeName<std::pair<T, T2>, void> {
    static std::string name() { return "std::pair<" + TypeName<T>::name() + ", " + TypeName<T2>::name() + ">"; }
};
template <typename T>
struct TypeName<std::vector<T>, void> {
    static std::string name() { return "std::vector<" + TypeName<T>::name() + ">"; }
};
template <>
struct TypeName<std::string, void> {
    static std::string name() { return "std::string"; }
};
template <>
struct TypeName<std::u8string, void> {
    static std::string name() { return "std::u8string"; }
};
template <>
struct TypeName<std::u16string, void> {
    static std::string name() { return "std::u16string"; }
};
template <>
struct TypeName<std::u32string, void> {
    static std::string name() { return "std::u32string"; }
};
template <>
struct TypeName<std::byte, void> {
    static std::string name() { return "std::byte"; }
};
template <>
struct TypeName<std::nullptr_t, void> {
    static std::string name() { return "std::nullptr_t"; }
};
template <>
struct TypeName<bool, void> {
    static std::string name() { return "bool"; }
};
template <>
struct TypeName<int, void> {
    static std::string name() { return "int"; }
};
template <>
struct TypeName<unsigned int, void> {
    static std::string name() { return "unsigned int"; }
};
template <>
struct TypeName<long, void> {
    static std::string name() { return "long"; }
};
template <>
struct TypeName<unsigned long, void> {
    static std::string name() { return "unsigned long"; }
};
template <>
struct TypeName<long long, void> {
    static std::string name() { return "long long"; }
};
template <>
struct TypeName<unsigned long long, void> {
    static std::string name() { return "unsigned long long"; }
};
template <>
struct TypeName<float, void> {
    static std::string name() { return "float"; }
};
template <>
struct TypeName<double, void> {
    static std::string name() { return "double"; }
};
template <typename T>
struct TypeName<T*, void> {
    static std::string name() { return TypeName<T>::name() + "*"; }
};
template <typename T>
struct TypeName<T&, void> {
    static std::string name() { return TypeName<T>::name() + "&"; }
};
template <typename T>
struct TypeName<const T&, void> {
    static std::string name() { return "const " + TypeName<T>::name() + "&"; }
};
template <typename T>
struct TypeName<T&&, void> {
    static std::string name() { return TypeName<T>::name() + "&&"; }
};
template <typename T>
struct TypeName<const T, void> {
    static std::string name() { return "const " + TypeName<T>::name(); }
};

template <typename T, ConstexprString... ArgNames>
struct SerializerHelperObject {
    T& mTuple;
    template <typename Serializer>
    bool save(Serializer& sa) const NEKO_NOEXCEPT {
        if constexpr (sizeof...(ArgNames) > 0) {
            bool ret                                                        = sa.startObject(sizeof...(ArgNames));
            std::array<std::string_view, sizeof...(ArgNames)> argNamesArray = {ArgNames.view()...};
            ret = [&sa, &argNamesArray, this]<std::size_t... Is>(std::index_sequence<Is...>) {
                return ((sa(make_name_value_pair(argNamesArray[Is], std::get<Is>(mTuple)))) && ...);
            }(std::make_index_sequence<sizeof...(ArgNames)>{});
            return ret && sa.endObject();
        } else {
            return sa(mTuple);
        }
    }

    template <typename Serializer>
    bool load(Serializer& sa) NEKO_NOEXCEPT {
        if (sa.isObject()) {
            if constexpr (sizeof...(ArgNames) > 0) {
                bool ret = sa.startNode();
                if (ret) {
                    std::array<std::string_view, sizeof...(ArgNames)> argNamesArray = {ArgNames.view()...};
                    ret = [&sa, &argNamesArray, this]<std::size_t... Is>(std::index_sequence<Is...>) {
                        return ((sa(make_name_value_pair(argNamesArray[Is], std::get<Is>(mTuple)))) && ...);
                    }(std::make_index_sequence<sizeof...(ArgNames)>{});
                }
                if (sa.finishNode() && ret) {
                    return ret;
                }
            }
            return false;
        }
        return sa(mTuple);
    }

    struct Neko {
        static constexpr std::array<std::string_view, sizeof...(ArgNames)> names = {ArgNames.view()...}; // NOLINT
        static constexpr std::tuple values = []<std::size_t... Is>(std::index_sequence<Is...>) {         // NOLINT
            return std::tuple{([](auto&& self) -> auto& { return std::get<Is>(self.mTuple); })...};
        }(std::make_index_sequence<sizeof...(ArgNames)>{});
    };
};
} // namespace traits

NEKO_END_NAMESPACE