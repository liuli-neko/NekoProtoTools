/**
 * @file jsonrpc_traits.hpp
 */
#pragma once

#include "nekoproto/rpc/traits.hpp"
#include "nekoproto/serialization/json_serializer.hpp"
#include "nekoproto/serialization/private/traits.hpp"
#include "nekoproto/serialization/reflection.hpp"
#include "nekoproto/serialization/serializer_base.hpp"

NEKO_BEGIN_NAMESPACE
namespace traits {

template <typename T>
concept Serializable =
    NEKO_NAMESPACE::detail::parser_serializable<JsonSerializer::Reader, JsonSerializer::Writer, std::decay_t<T>>;

template <typename T, class enable = void>
struct IsSerializable : std::false_type {};

template <Serializable T>
struct IsSerializable<T, void> : std::true_type {};

template <>
struct IsSerializable<void> : std::true_type {};

} // namespace traits

namespace detail {

template <typename T, ConstexprString... ArgNames>
struct JsonRpcSerializerHelperObject {
    T& mTuple;

    struct Neko {
        static constexpr std::array<std::string_view, sizeof...(ArgNames)> names = {ArgNames.view()...};
        static constexpr std::tuple values = []<std::size_t... Is>(std::index_sequence<Is...>) {
            return std::tuple{([](auto&& self) -> auto& { return std::get<Is>(self.mTuple); })...};
        }(std::make_index_sequence<sizeof...(ArgNames)>{});
    };
};

template <typename... Args>
constexpr bool jsonrpc_automatic_expansion_able() {
    if constexpr (sizeof...(Args) == 1) {
        return has_values_meta<std::remove_cvref_t<std::tuple_element_t<0, std::tuple<Args...>>>> &&
               has_names_meta<std::remove_cvref_t<std::tuple_element_t<0, std::tuple<Args...>>>>;
    } else {
        return false;
    }
}

template <typename T, class enable = void>
struct jsonrpc_first_type_in_tuple {
    using type = void;
};

template <typename T, typename... Ts>
struct jsonrpc_first_type_in_tuple<std::tuple<T, Ts...>> {
    using type = T;
};

template <typename T, class enable = void>
struct jsonrpc_is_null_able_object_helper;

template <typename T>
consteval bool jsonrpc_is_null_able_object() {
    if constexpr (NEKO_NAMESPACE::detail::is_optional<T>::value) {
        return true;
    } else if constexpr (jsonrpc_automatic_expansion_able<T>()) {
        using types = Reflect<T>::value_types;
        return jsonrpc_is_null_able_object_helper<types>::value;
    } else if constexpr (is_std_tuple_v<T>) {
        return jsonrpc_is_null_able_object_helper<T>::value;
    } else {
        return false;
    }
}

template <typename T, class enable>
struct jsonrpc_is_null_able_object_helper {
    static constexpr bool value = jsonrpc_is_null_able_object<T>();
};

template <typename... Ts>
struct jsonrpc_is_null_able_object_helper<std::tuple<Ts...>> {
    static constexpr bool value = (jsonrpc_is_null_able_object<Ts>() && ...);
};

template <typename MethodTraits>
struct JsonRpcMethodTraits {
    using RawParamsType = typename MethodTraits::RawParamsType;
    using RawReturnType = typename MethodTraits::RawReturnType;

    template <typename Tuple>
    struct Impl;

    template <typename... Args>
    struct Impl<std::tuple<Args...>> {
        using DecayTuple = std::tuple<std::remove_cvref_t<Args>...>;
        using FirstType  = typename jsonrpc_first_type_in_tuple<DecayTuple>::type;

        constexpr static bool is_auto_expand = jsonrpc_automatic_expansion_able<Args...>();

        template <char ch = 0>
        static constexpr bool _is_single_optional_auto_expand_arg() {
            if constexpr (sizeof...(Args) == 1) {
                if constexpr (traits::optional_like_type<FirstType>::value && !std::is_void_v<FirstType>) {
                    return jsonrpc_automatic_expansion_able<typename traits::optional_like_type<FirstType>::type>();
                } else {
                    return false;
                }
            } else {
                return false;
            }
        }

        constexpr static bool is_single_optional_auto_expand_arg = _is_single_optional_auto_expand_arg();

        template <char ch = 0>
        static constexpr bool _is_single_tuple_arg() {
            if constexpr (sizeof...(Args) == 1) {
                return is_std_tuple_v<FirstType>;
            } else {
                return false;
            }
        }

        constexpr static bool is_single_tuple_arg = _is_single_tuple_arg();

        using ParamsTupleType = decltype([] {
            if constexpr (is_auto_expand || is_single_optional_auto_expand_arg) {
                return std::remove_cvref_t<FirstType>{};
            } else if constexpr (is_single_tuple_arg) {
                return std::remove_cvref_t<FirstType>{};
            } else {
                return std::tuple<std::remove_cvref_t<Args>...>{};
            }
        }());

        template <char ch = 0>
        constexpr static int _params_size() {
            if constexpr (is_auto_expand) {
                return Reflect<FirstType>::size();
            } else if constexpr (is_single_optional_auto_expand_arg && !std::is_void_v<FirstType>) {
                return Reflect<typename traits::optional_like_type<FirstType>::type>::size();
            } else if constexpr (is_single_tuple_arg) {
                return std::tuple_size_v<FirstType>;
            } else {
                return sizeof...(Args);
            }
        }

        constexpr static int ParamsSize                = _params_size();
        constexpr static bool IsNullAble               = jsonrpc_is_null_able_object<ParamsTupleType>();
        constexpr static bool IsAutomaticExpansionAble = is_auto_expand || is_single_optional_auto_expand_arg;
        constexpr static bool IsTopTuple               = is_single_tuple_arg;
    };

    using ParamsTupleType = typename Impl<RawParamsType>::ParamsTupleType;
    using ReturnType = std::optional<std::conditional_t<std::is_void_v<RawReturnType>, std::nullptr_t,
                                                        std::remove_cvref_t<RawReturnType>>>;

    constexpr static int NumParams                 = MethodTraits::NumParams;
    constexpr static int ParamsSize                = Impl<RawParamsType>::ParamsSize;
    constexpr static bool IsNullAble               = Impl<RawParamsType>::IsNullAble;
    constexpr static bool IsAutomaticExpansionAble = Impl<RawParamsType>::IsAutomaticExpansionAble;
    constexpr static bool IsTopTuple               = Impl<RawParamsType>::IsTopTuple;
};

} // namespace detail
NEKO_END_NAMESPACE
