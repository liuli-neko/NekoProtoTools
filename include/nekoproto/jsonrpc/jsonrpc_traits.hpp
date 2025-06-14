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

#include <ilias/task.hpp>
#include <string_view>

#include "nekoproto/global/reflect.hpp"
#include "nekoproto/jsonrpc/jsonrpc_error.hpp"
#include "nekoproto/serialization/json_serializer.hpp"
#include "nekoproto/serialization/serializer_base.hpp"
#include "nekoproto/serialization/types/types.hpp"

NEKO_BEGIN_NAMESPACE
namespace traits {
template <typename T>
concept Serializable = requires(JsonSerializer::OutputSerializer serializer, T value) {
    { serializer(value) };
};
template <typename T, class enable = void>
struct IsSerializable : std::false_type {};
template <Serializable T>
struct IsSerializable<T, void> : std::true_type {};
template <>
struct IsSerializable<void> : std::true_type {};

template <typename T, class enable = void>
struct TypeName {
    constexpr static std::string name() { return std::string(NEKO_NAMESPACE::detail::class_nameof<T>); }
};

template <ConstexprString... ArgNames>
struct ArgNamesHelper {};

template <typename RawParamsType, std::size_t... Is, ConstexprString... ArgNames>
constexpr auto parameter_to_string(std::index_sequence<Is...> /*unused*/, ArgNamesHelper<ArgNames...> /*unused*/ = {})
    -> std::string_view {
    if constexpr (sizeof...(Is) == 0) {
        return "";
    } else if constexpr (sizeof...(ArgNames) != sizeof...(Is)) {
        constexpr auto joined_arr = []() {
            constexpr std::size_t Nt                                 = sizeof...(Is) - 1;
            constexpr std::array<std::string_view, Nt + 1> argsNames = {
                traits::TypeName<std::tuple_element_t<Is, RawParamsType>>::name()...};
            constexpr std::size_t len =
                (traits::TypeName<std::tuple_element_t<Is, RawParamsType>>::name().size() + ...) + Nt * 2;
            std::array<char, len + 1> arr;
            auto append = [idx = 0, &arr](const auto& str) mutable {
                for (auto ch : str) {
                    arr[idx++] = ch;
                }
                if (idx + 1 < arr.size()) {
                    arr[idx++] = ',';
                    arr[idx++] = ' ';
                }
            };
            (append(traits::TypeName<std::tuple_element_t<Is, RawParamsType>>::name()), ...);
            arr[len] = '\0';
            return arr;
        }();
        auto& static_arr = NEKO_NAMESPACE::detail::make_static<joined_arr>::value;
        return {static_arr.data(), static_arr.size() - 1};
    } else {
        constexpr auto joined_arr = []() {
            constexpr std::size_t Nt                                 = sizeof...(Is) - 1;
            constexpr std::array<std::string_view, Nt + 1> argsNames = {ArgNames.view()...};
            constexpr std::size_t len                                = (ArgNames.view().size() + ...) + Nt * 2;
            std::array<char, len + 1> arr;
            auto append = [idx = 0, &arr](const auto& str) mutable {
                for (auto ch : str) {
                    arr[idx++] = ch;
                }
                if (idx + 1 < arr.size()) {
                    arr[idx++] = ',';
                    arr[idx++] = ' ';
                }
            };
            (append(ArgNames.view()), ...);
            arr[len] = '\0';
            return arr;
        }();
        auto& static_arr = NEKO_NAMESPACE::detail::make_static<joined_arr>::value;
        return {static_arr.data(), static_arr.size() - 1};
    }
};
template <>
struct TypeName<void, void> {
    constexpr static std::string_view name() { return std::string_view("void"); }
};
template <typename T>
struct TypeName<std::optional<T>, void> {
    constexpr static std::string_view name() {
        return join_v<std::string_view("std::optional<"), TypeName<T>::name(), std::string_view(">")>;
    }
};
template <typename... Args>
struct TypeName<std::variant<Args...>, void> {
    constexpr static std::string_view name() {
        return join_v<std::string_view("std::variant<"),
                      parameter_to_string<std::tuple<Args...>>(std::make_index_sequence<sizeof...(Args)>{}),
                      std::string_view(">")>;
    }
};
template <typename T, std::size_t N>
struct TypeName<std::array<T, N>, void> {
    constexpr static std::string_view name() {
        return join_v<std::string_view("std::array<"), TypeName<T>::name(), std::string_view(", "), std::to_string(N),
                      std::string_view(">")>;
    }
};
template <typename... Args>
struct TypeName<std::tuple<Args...>, void> {
    constexpr static std::string_view name() {
        return join_v<std::string_view("std::tuple<"),
                      parameter_to_string<std::tuple<Args...>>(std::make_index_sequence<sizeof...(Args)>{}),
                      std::string_view(">")>;
    }
};
template <typename T, typename T2>
struct TypeName<std::pair<T, T2>, void> {
    constexpr static std::string_view name() {

        return join_v<std::string_view("std::pair<"), TypeName<T>::name(), std::string_view(", "), TypeName<T2>::name(),
                      std::string_view(">")>;
    }
};
template <typename T>
struct TypeName<std::vector<T>, void> {
    constexpr static std::string_view name() {
        return join_v<std::string_view("std::vector<"), TypeName<T>::name(), std::string_view(">")>;
    }
};
template <>
struct TypeName<std::string, void> {
    constexpr static std::string_view name() { return std::string_view("std::string"); }
};
template <>
struct TypeName<std::u8string, void> {
    constexpr static std::string_view name() { return std::string_view("std::u8string"); }
};
template <>
struct TypeName<std::u16string, void> {
    constexpr static std::string_view name() { return std::string_view("std::u16string"); }
};
template <>
struct TypeName<std::u32string, void> {
    constexpr static std::string_view name() { return std::string_view("std::u32string"); }
};
template <>
struct TypeName<std::byte, void> {
    constexpr static std::string_view name() { return std::string_view("std::byte"); }
};
template <>
struct TypeName<std::nullptr_t, void> {
    constexpr static std::string_view name() { return std::string_view("std::nullptr_t"); }
};
template <>
struct TypeName<bool, void> {
    constexpr static std::string_view name() { return std::string_view("bool"); }
};
template <>
struct TypeName<int, void> {
    constexpr static std::string_view name() { return std::string_view("int"); }
};
template <>
struct TypeName<unsigned int, void> {
    constexpr static std::string_view name() { return std::string_view("unsigned int"); }
};
template <>
struct TypeName<long, void> {
    constexpr static std::string_view name() { return std::string_view("long"); }
};
template <>
struct TypeName<unsigned long, void> {
    constexpr static std::string_view name() { return std::string_view("unsigned long"); }
};
template <>
struct TypeName<long long, void> {
    constexpr static std::string_view name() { return std::string_view("long long"); }
};
template <>
struct TypeName<unsigned long long, void> {
    constexpr static std::string_view name() { return std::string_view("unsigned long long"); }
};
template <>
struct TypeName<float, void> {
    constexpr static std::string_view name() { return std::string_view("float"); }
};
template <>
struct TypeName<double, void> {
    constexpr static std::string_view name() { return std::string_view("double"); }
};
template <typename T>
struct TypeName<T*, void> {
    constexpr static std::string_view name() { return join_v<TypeName<T>::name(), std::string_view("*")>; }
};
template <typename T>
struct TypeName<T&, void> {
    constexpr static std::string_view name() { return join_v<TypeName<T>::name(), std::string_view("&")>; }
};
template <typename T>
struct TypeName<const T&, void> {
    constexpr static std::string_view name() {
        return join_v<std::string_view("const "), TypeName<T>::name(), std::string_view("&")>;
    }
};
template <typename T>
struct TypeName<T&&, void> {
    constexpr static std::string_view name() { return join_v<TypeName<T>::name(), std::string_view("&&")>; }
};
template <typename T>
struct TypeName<const T, void> {
    constexpr static std::string_view name() { return join_v<std::string_view("const "), TypeName<T>::name()>; }
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