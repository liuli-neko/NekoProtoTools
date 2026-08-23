/**
 * @file jsonrpc_traits.hpp
 */
#pragma once

#include <span>
#include <string>

#include "nekoproto/global/traits.hpp"
#include "nekoproto/rpc/traits.hpp"
#include "nekoproto/serialization/json_serializer.hpp"
#include "nekoproto/serialization/reflection.hpp"
#include "nekoproto/serialization/serializer_base.hpp"

namespace nekoproto {
namespace traits {

template <typename T>
concept Serializable =
    nekoproto::detail::parser_serializable<JsonSerializer::Reader, JsonSerializer::Writer, std::decay_t<T>>;

template <typename T, class enable = void>
struct IsSerializable : std::false_type {};

template <Serializable T>
struct IsSerializable<T, void> : std::true_type {};

template <>
struct IsSerializable<void> : std::true_type {};

} // namespace traits

namespace detail {

struct JsonRpcMethodContext {
    std::span<const std::string> argNames{};
};

template <typename T>
struct JsonRpcSerializerHelperObject {
    T& mTuple;
    JsonRpcMethodContext context{};
};

template <typename... Args>
constexpr auto jsonrpcAutomaticExpansionAble() -> bool {
    if constexpr (sizeof...(Args) == 1) {
        return has_values_meta<std::remove_cvref_t<std::tuple_element_t<0, std::tuple<Args...>>>> &&
               has_names_meta<std::remove_cvref_t<std::tuple_element_t<0, std::tuple<Args...>>>>;
    } else {
        return false;
    }
}

template <typename T, class enable = void>
struct JsonrpcFirstTypeInTuple {
    using type = void;
};

template <typename T, typename... Ts>
struct JsonrpcFirstTypeInTuple<std::tuple<T, Ts...>> {
    using type = T;
};

template <typename T, class enable = void>
struct JsonrpcIsNullAbleObjectHelper;

template <typename T>
consteval auto jsonrpcIsNullAbleObject() -> bool {
    if constexpr (nekoproto::detail::IsOptional<T>::value) {
        return true;
    } else if constexpr (jsonrpcAutomaticExpansionAble<T>()) {
        using types = Reflect<T>::value_types;
        return JsonrpcIsNullAbleObjectHelper<types>::value;
    } else if constexpr (is_std_tuple_v<T>) {
        return JsonrpcIsNullAbleObjectHelper<T>::value;
    } else {
        return false;
    }
}

template <typename T, class enable>
struct JsonrpcIsNullAbleObjectHelper {
    static constexpr bool value = jsonrpcIsNullAbleObject<T>();
};

template <typename... Ts>
struct JsonrpcIsNullAbleObjectHelper<std::tuple<Ts...>> {
    static constexpr bool value = (jsonrpcIsNullAbleObject<Ts>() && ...);
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
        using FirstType  = typename JsonrpcFirstTypeInTuple<DecayTuple>::type;

        constexpr static bool is_auto_expand = jsonrpcAutomaticExpansionAble<Args...>();

        template <char ch = 0>
        static constexpr auto isSingleOptionalAutoExpandArg() -> bool {
            if constexpr (sizeof...(Args) == 1) {
                if constexpr (traits::OptionalLikeType<FirstType>::value && !std::is_void_v<FirstType>) {
                    return jsonrpcAutomaticExpansionAble<typename traits::OptionalLikeType<FirstType>::type>();
                } else {
                    return false;
                }
            } else {
                return false;
            }
        }

        constexpr static bool is_single_optional_auto_expand_arg = isSingleOptionalAutoExpandArg();

        template <char ch = 0>
        static constexpr auto isSingleTupleArg() -> bool {
            if constexpr (sizeof...(Args) == 1) {
                return is_std_tuple_v<FirstType>;
            } else {
                return false;
            }
        }

        constexpr static bool is_single_tuple_arg = isSingleTupleArg();

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
        constexpr static auto paramsSize() -> int {
            if constexpr (is_auto_expand) {
                return Reflect<FirstType>::size();
            } else if constexpr (is_single_optional_auto_expand_arg && !std::is_void_v<FirstType>) {
                return Reflect<typename traits::OptionalLikeType<FirstType>::type>::size();
            } else if constexpr (is_single_tuple_arg) {
                return std::tuple_size_v<FirstType>;
            } else {
                return sizeof...(Args);
            }
        }

        constexpr static int ParamsSize                = paramsSize();
        constexpr static bool IsNullAble               = jsonrpcIsNullAbleObject<ParamsTupleType>();
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
} // namespace nekoproto
