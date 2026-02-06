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
#include <tuple>
#include <type_traits>

#include "ilias/defines.hpp"
#include "nekoproto/global/config.h"
#include "nekoproto/global/reflect.hpp"
#include "nekoproto/jsonrpc/jsonrpc_error.hpp"
#include "nekoproto/serialization/json/schema.hpp"
#include "nekoproto/serialization/json_serializer.hpp"
#include "nekoproto/serialization/private/traits.hpp"
#include "nekoproto/serialization/serializer_base.hpp"
#include "nekoproto/serialization/types/types.hpp"
#include "nekoproto/serialization/types/unordered_map.hpp"

NEKO_BEGIN_NAMESPACE
namespace detail {
// 特化：普通函数指针
template <typename R, typename... Args>
struct function_traits<ILIAS_NAMESPACE::IoTask<R> (*)(Args...), void> {
    using return_type = R;
    using arg_tuple   = std::tuple<Args...>;
    template <typename Ret, template <typename...> class T>
    using args_in = T<Ret, Args...>;
};

} // namespace detail
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
    }
    std::string result;
    auto appendParam = [&](auto idx) {
        constexpr size_t Idx = idx;
        using ParamType      = std::tuple_element_t<Idx, RawParamsType>;

        if constexpr (sizeof...(ArgNames) == sizeof...(Is)) {
            constexpr std::array<std::string_view, sizeof...(Is)> CArgNames = {ArgNames.view()...};
            result += TypeName<ParamType>::name() + " " + std::string(CArgNames[Idx]);
        } else {
            result += TypeName<ParamType>::name();
        }

        if constexpr (Idx < sizeof...(Is) - 1) {
            result += ", ";
        }
    };
    (appendParam(std::integral_constant<size_t, Is>{}), ...);
    return result;
};
template <>
struct TypeName<void, void> {
    static std::string name() { return "void"; }
};
template <typename T>
struct TypeName<std::optional<T>, void> {
    static std::string name() { return std::format("std::optional<{}>", TypeName<T>::name()); }
};
template <typename... Args>
struct TypeName<std::variant<Args...>, void> {
    static std::string name() {
        return std::format("std::variant<{}>",
                           parameter_to_string<std::tuple<Args...>>(std::make_index_sequence<sizeof...(Args)>{}));
    }
};
template <typename T, std::size_t N>
struct TypeName<std::array<T, N>, void> {
    static std::string name() { return std::format("std::array<{}, {}>", TypeName<T>::name(), N); }
};
template <typename... Args>
struct TypeName<std::tuple<Args...>, void> {
    static std::string name() {
        return std::format("std::tuple<{}>",
                           parameter_to_string<std::tuple<Args...>>(std::make_index_sequence<sizeof...(Args)>{}));
    }
};
template <typename T, typename T2>
struct TypeName<std::pair<T, T2>, void> {
    static std::string name() { return std::format("std::pair<{}, {}>", TypeName<T>::name(), TypeName<T2>::name()); }
};
template <typename T>
struct TypeName<std::vector<T>, void> {
    static std::string name() { return std::format("std::vector<{}>", TypeName<T>::name()); }
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
    static std::string name() { return std::format("const {}&", TypeName<T>::name()); }
};
template <typename T>
struct TypeName<T&&, void> {
    static std::string name() { return std::format("{}&&", TypeName<T>::name()); }
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

#if __cpp_lib_move_only_function >= 202110L
template <typename... ArgsT>
using FunctionT = std::move_only_function<ArgsT...>;
#else
template <typename... ArgsT>
using FunctionT = std::function<ArgsT...>;
#endif

using NEKO_NAMESPACE::detail::function_traits;
using NEKO_NAMESPACE::detail::has_names_meta;
using NEKO_NAMESPACE::detail::has_values_meta;
using NEKO_NAMESPACE::detail::is_std_tuple_v;

template <typename... Args>
constexpr bool is_automatic_expansion_able() {
    if constexpr (sizeof...(Args) == 1) {
        return has_values_meta<std::remove_cvref_t<std::tuple_element_t<0, std::tuple<Args...>>>> &&
               has_names_meta<std::remove_cvref_t<std::tuple_element_t<0, std::tuple<Args...>>>>;
    } else {
        return false;
    }
}

template <typename... Args>
constexpr bool is_automatic_expansion_able_v = is_automatic_expansion_able<Args...>(); // NOLINT

template <typename RetT, typename... Args>
class RpcMethodExecutor {
public:
    using RawReturnType      = RetT;
    using CoroutinesFuncType = FunctionT<ILIAS_NAMESPACE::IoTask<RawReturnType>(Args...)>;

    auto operator()(Args... args) const -> ILIAS_NAMESPACE::IoTask<RawReturnType> {
        if (mCoFunction) {
            co_return co_await mCoFunction(std::forward<Args>(args)...);
        }
        co_return ILIAS_NAMESPACE::Unexpected(JsonRpcError::MethodNotBind);
    }

    auto notification(Args... args) const -> ILIAS_NAMESPACE::IoTask<void> {
        struct NotificationGuard {
            bool& flag;
            NotificationGuard(bool& fg) : flag(fg) { flag = true; }
            ~NotificationGuard() { flag = false; }
        };
        NotificationGuard guard{mIsNotification};

        if (mCoFunction) {
            if (auto ret = co_await mCoFunction(std::forward<Args>(args)...); !ret) {
                co_return ILIAS_NAMESPACE::Unexpected(ret.error());
            }
        }
        co_return {};
    }

    auto isNotification() const -> bool { return mIsNotification; }

protected:
    mutable CoroutinesFuncType mCoFunction;
    mutable bool mIsNotification = false;
};

template <typename T, class enable = void>
struct is_null_able_object_helper;

template <typename T>
consteval bool is_null_able_object() {
    if constexpr (NEKO_NAMESPACE::detail::is_optional<T>::value) {
        return true;
    } else if constexpr (is_automatic_expansion_able<T>()) {
        using types     = Reflect<T>::value_types;
        return is_null_able_object_helper<types>::value;
    } else if constexpr (is_std_tuple_v<T>) {
        return is_null_able_object_helper<T>::value;
    } else {
        return false;
    }
}

template <typename T, class enable>
struct is_null_able_object_helper {
    static constexpr bool value = is_null_able_object<T>();
};

template <typename ...Ts>
struct is_null_able_object_helper<std::tuple<Ts...>> {
    static constexpr bool value = (is_null_able_object<Ts>() && ...);
};

template <typename Traits, typename... T>
struct RpcMethodAliasesHelper {
    using DecayTuple                                         = std::tuple<std::remove_cvref_t<T>...>;
    constexpr static bool is_auto_expand                     = is_automatic_expansion_able<T...>(); // NOLINT
    constexpr static bool is_single_optional_auto_expand_arg = [] {                                 // NOLINT
        if constexpr (sizeof...(T) == 1) {
            using FirstArg = std::tuple_element_t<0, DecayTuple>;
            if constexpr (optional_like<FirstArg>) {
                return is_automatic_expansion_able<typename optional_like_type<FirstArg>::type>();
            } else {
                return false;
            }
        } else {
            return false;
        }
    }();
    constexpr static bool is_single_tuple_arg = [] { // NOLINT
        if constexpr (sizeof...(T) == 1) {
            using FirstArg = std::tuple_element_t<0, DecayTuple>;
            return is_std_tuple_v<std::remove_cvref_t<FirstArg>>;
        } else {
            return false;
        }
    }();

    /**
     * @brief Type of the parameters to be passed to the RPC method.
     * I want to auto expand types like struct XxParams {int a, std::string b} to "params": {"a": 1, "b": "hello"} in
     * request. I will expand like this:
     * 1. a type has name, value in reflect meta. like struct XxParams {int a, std::string b}.
     * 2. a type in optional and has name, value in reflect meta. like std::optional<XxParams>.
     * for other args, I will make to std::tuple<...>. of course, can use std::tuple<...> directly.
     */
    using ParamsTupleType = decltype([] {
        if constexpr (is_auto_expand ||
                      is_single_optional_auto_expand_arg) { // is_auto_expand implies sizeof...(T) == 1
            using FirstArg = std::tuple_element_t<0, DecayTuple>;
            static_assert(traits::IsSerializable<FirstArg>::value,
                          "The params of bound functions must be serializable");
            return std::remove_cvref_t<FirstArg>{}; // Return an object of the desired type
        } else if constexpr (is_single_tuple_arg) {
            using FirstArg = std::tuple_element_t<0, DecayTuple>;
            static_assert(traits::IsSerializable<FirstArg>::value,
                          "The params of bound functions must be serializable");
            return std::remove_cvref_t<FirstArg>{};
        } else {
            static_assert((traits::IsSerializable<T>::value && ...),
                          "The params of bound functions must be serializable");
            return std::tuple<std::remove_cvref_t<T>...>{};
        }
    }()); // Use decltype on the lambda's return type to get the actual type

    using RawParamsType = std::tuple<T...>;
    using RetT          = typename Traits::return_type;
    static_assert(traits::IsSerializable<RetT>::value,
                  "The return type T or IoTask<T> of bound functions must be serializable");
    using FunctionType = FunctionT<RetT(T...)>;

    constexpr static int NumParams  = sizeof...(T);
    constexpr static int ParamsSize = [] {
        if constexpr (is_auto_expand) {
            return Reflect<std::tuple_element_t<0, DecayTuple>>::size();
        } else if constexpr (is_single_optional_auto_expand_arg) {
            using FirstArg = std::tuple_element_t<0, DecayTuple>;
            return Reflect<typename optional_like_type<FirstArg>::type>::size();
        } else if constexpr (is_single_tuple_arg) {
            return std::tuple_size_v<std::remove_cvref_t<std::tuple_element_t<0, DecayTuple>>>;
        } else {
            return sizeof...(T);
        }
    }();
    constexpr static bool IsNullAble = is_null_able_object<ParamsTupleType>();
    constexpr static bool IsAutomaticExpansionAble = is_auto_expand || is_single_optional_auto_expand_arg;
    constexpr static bool IsTopTuple               = is_single_tuple_arg;
};

template <typename Traits, typename Tuple>
struct RpcMethodAliases;

template <typename Traits, typename... T>
struct RpcMethodAliases<Traits, std::tuple<T...>> {
    using type = RpcMethodAliasesHelper<Traits, T...>;
};

template <typename Callable>
using RpcMethodTraitsUnpacker =
    typename function_traits<Callable>::template args_in<typename function_traits<Callable>::return_type,
                                                         RpcMethodExecutor>;

} // namespace traits

NEKO_END_NAMESPACE