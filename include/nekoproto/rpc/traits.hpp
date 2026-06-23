#pragma once

#include <array>
#include <concepts>
#include <functional>
#include <ilias/task.hpp>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <variant>
#include <vector>

#include "nekoproto/global/config.h"
#include "nekoproto/global/reflect.hpp"
#include "nekoproto/rpc/error.hpp"

NEKO_BEGIN_NAMESPACE

namespace detail {
template <typename R, typename... Args>
struct function_traits<ilias::IoTask<R> (*)(Args...), void> {
    using return_type = R;
    using arg_tuple   = std::tuple<Args...>;
    template <typename Ret, template <typename...> class T>
    using args_in = T<Ret, Args...>;
};
} // namespace detail

namespace traits {

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

#if __cpp_lib_move_only_function >= 202110L
template <typename... ArgsT>
using FunctionT = std::move_only_function<ArgsT...>;
#else
template <typename... ArgsT>
using FunctionT = std::function<ArgsT...>;
#endif

using NEKO_NAMESPACE::detail::function_traits;

template <typename RetT, typename... Args>
class RpcMethodExecutor {
public:
    using RawReturnType      = RetT;
    using CoroutinesFuncType = FunctionT<ilias::IoTask<RawReturnType>(Args...)>;

    auto operator()(Args... args) const -> ilias::IoTask<RawReturnType> {
        if (mCoFunction) {
            co_return co_await mCoFunction(std::forward<Args>(args)...);
        }
        co_return ilias::Err(RpcError::MethodNotBind);
    }

    auto notification(Args... args) const -> ilias::IoTask<void> {
        struct NotificationGuard {
            bool& flag;
            explicit NotificationGuard(bool& fg) : flag(fg) { flag = true; }
            ~NotificationGuard() { flag = false; }
        };
        NotificationGuard guard{mIsNotification};

        if (mCoFunction) {
            if (auto ret = co_await mCoFunction(std::forward<Args>(args)...); !ret) {
                co_return ilias::Err(ret.error());
            }
        }
        co_return {};
    }

    auto isNotification() const -> bool { return mIsNotification; }

protected:
    mutable CoroutinesFuncType mCoFunction;
    mutable bool mIsNotification = false;
};

template <typename RetT, typename... Args>
class RpcMethodMetadata : public RpcMethodExecutor<RetT, Args...> {
public:
    using RawReturnType = RetT;
    using RawParamsType = std::tuple<Args...>;
    using FunctionType  = FunctionT<RetT(Args...)>;

    constexpr static int NumParams = sizeof...(Args);
};

template <typename Callable>
using RpcMethodTraitsUnpacker =
    typename function_traits<Callable>::template args_in<typename function_traits<Callable>::return_type,
                                                         RpcMethodMetadata>;

} // namespace traits

NEKO_END_NAMESPACE
